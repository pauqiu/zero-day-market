#include "infrastructure/db/mariadb_account_repository.h"

#include <vector>

#include "infrastructure/db/database_error.h"

namespace {

constexpr unsigned int kDuplicateEntry = 1062;  // ER_DUP_ENTRY

#define EPOCH_OF(column) "TIMESTAMPDIFF(SECOND, '1970-01-01 00:00:00', " column ")"
#define FROM_EPOCH "TIMESTAMPADD(SECOND, ?, '1970-01-01 00:00:00')"

// New accounts wait for the email verification (HU-07).
constexpr const char* kInsertUserSql =
    "INSERT INTO users (username, email, password_hash, status) VALUES (?, ?, ?, 'pending')";

constexpr const char* kInsertYubikeySql =
    "INSERT INTO yubikeys (user_id, public_id, aes_key_enc, private_id_enc, "
    "                      last_counter, last_session_use, registered_at) "
    "VALUES (?, ?, ?, ?, ?, ?, UTC_TIMESTAMP())";

constexpr const char* kInsertTokenSql =
    "INSERT INTO email_verifications (token_hash, user_id, expires_at) VALUES (?, ?, " FROM_EPOCH ")";

constexpr const char* kInsertAuditSql =
    "INSERT INTO audit_log (created_at, ip, category, description, user_id, details) "
    "VALUES (UTC_TIMESTAMP(3), ?, ?, ?, ?, ?)";

constexpr const char* kInsertIncidentSql =
    "INSERT INTO security_incidents (created_at, ip, category, description, details) "
    "VALUES (UTC_TIMESTAMP(3), ?, ?, ?, ?)";

// Activation. FOR UPDATE locks the token and the user until the transaction
// ends, so the same link opened twice at the same time activates only once.
constexpr const char* kLockTokenSql =
    "SELECT ev.user_id, u.username, u.status, ev.used_at IS NULL, " EPOCH_OF("ev.expires_at") " "
    "FROM email_verifications ev JOIN users u ON u.user_id = ev.user_id "
    "WHERE ev.token_hash = ? FOR UPDATE";

constexpr const char* kUseTokenSql =
    "UPDATE email_verifications SET used_at = " FROM_EPOCH " WHERE token_hash = ?";

constexpr const char* kActivateUserSql =
    "UPDATE users SET status = 'active' WHERE user_id = ? AND status = 'pending'";

// The initial allotment: the lowest IDs of the inventory without owner,
// locked so a simultaneous activation cannot take the same objects (it
// waits, and then sees them already owned).
constexpr const char* kLockFreeObjectsSql =
    "SELECT object_id FROM objects WHERE owner_id IS NULL ORDER BY object_id LIMIT ? FOR UPDATE";

constexpr const char* kAssignObjectSql =
    "UPDATE objects SET owner_id = ?, last_transfer_at = " FROM_EPOCH " "
    "WHERE object_id = ? AND owner_id IS NULL";

constexpr const char* kInsertAllocationSql =
    "INSERT INTO transfers (object_id, from_user_id, to_user_id, type, offer_id, transferred_at) "
    "VALUES (?, NULL, ?, 'initial_allocation', NULL, " FROM_EPOCH ")";

constexpr const char* kFindPendingSql =
    "SELECT u.user_id, u.username, u.email, MAX(" EPOCH_OF("ev.expires_at") ") "
    "FROM users u LEFT JOIN email_verifications ev "
    "       ON ev.user_id = u.user_id AND ev.used_at IS NULL "
    "WHERE u.email = ? AND u.status = 'pending' "
    "GROUP BY u.user_id, u.username, u.email";

constexpr const char* kInvalidateTokensSql =
    "UPDATE email_verifications SET used_at = " FROM_EPOCH " WHERE user_id = ? AND used_at IS NULL";

CreateAccountOutcome duplicateOutcome(const std::string& message)
{
    if (message.find("uq_users_username") != std::string::npos) {
        return CreateAccountOutcome::UsernameTaken;
    }
    if (message.find("uq_users_email") != std::string::npos) {
        return CreateAccountOutcome::EmailTaken;
    }
    if (message.find("uq_yubikeys_public_id") != std::string::npos) {
        return CreateAccountOutcome::YubikeyTaken;
    }
    throw DatabaseError("unexpected duplicate key while creating an account");
}

void run(MariaDbConnection& connection, const char* sql, MariaDbParams& params, const char* what)
{
    MariaDbStatement statement(connection, sql);
    if (!params.execute(statement)) {
        statement.fail(what);
    }
}

}  // namespace

MariaDbAccountRepository::MariaDbAccountRepository(MariaDbConnection& connection)
    : connection(connection)
{
}

CreateAccountResult MariaDbAccountRepository::createAccount(const NewAccount& account)
{
    MariaDbTransaction transaction(connection);
    CreateAccountResult result;

    MariaDbStatement insertUser(connection, kInsertUserSql);
    MariaDbParams userParams;
    userParams.text(account.username).text(account.email).text(account.passwordHash);
    if (!userParams.execute(insertUser)) {
        if (insertUser.errorNumber() == kDuplicateEntry) {
            result.outcome = duplicateOutcome(insertUser.errorMessage());
            return result;  // the transaction rolls back
        }
        insertUser.fail("insert user");
    }
    const long long userID = static_cast<long long>(mysql_stmt_insert_id(insertUser.handle()));

    MariaDbStatement insertKey(connection, kInsertYubikeySql);
    MariaDbParams keyParams;
    keyParams.integer(userID)
        .text(account.publicID)
        .blob(account.aesKeySealed)
        .blob(account.privateIDSealed)
        .integer(account.counter)
        .integer(account.sessionUse);
    if (!keyParams.execute(insertKey)) {
        if (insertKey.errorNumber() == kDuplicateEntry) {
            result.outcome = duplicateOutcome(insertKey.errorMessage());
            return result;  // the user insert is rolled back too
        }
        insertKey.fail("insert yubikey");
    }

    MariaDbParams tokenParams;
    tokenParams.text(account.verificationTokenHash).integer(userID)
        .integer(account.verificationExpiresAt);
    run(connection, kInsertTokenSql, tokenParams, "insert verification token");

    MariaDbParams auditParams;
    auditParams.text(account.clientIP).text("cuenta").text("Registro de usuario con YubiKey")
        .integer(userID).text("public_id=" + account.publicID);
    run(connection, kInsertAuditSql, auditParams, "insert audit log");

    transaction.commit();
    result.outcome = CreateAccountOutcome::Created;
    result.userID = static_cast<int>(userID);
    return result;
}

ActivationResult MariaDbAccountRepository::activateAccount(const std::string& tokenHash,
                                                           std::int64_t now, int objects,
                                                           const std::string& clientIP)
{
    MariaDbTransaction transaction(connection);
    ActivationResult result;

    // 1. The token, locked. The statement is closed at the end of this block,
    //    before the next statements run on the same connection.
    long long userID = 0, unused = 0, expiresAt = 0;
    std::string username, status;
    {
        MariaDbStatement lockToken(connection, kLockTokenSql);
        MariaDbParams tokenParams;
        tokenParams.text(tokenHash);
        if (!tokenParams.execute(lockToken)) {
            lockToken.fail("lock token");
        }
        unsigned long lengths[5]{};
        my_bool nulls[5]{};
        MYSQL_BIND row[5]{};
        row[0].buffer_type = MYSQL_TYPE_LONGLONG; row[0].buffer = &userID; row[0].is_null = &nulls[0];
        row[1].buffer_type = MYSQL_TYPE_STRING;   row[1].length = &lengths[1]; row[1].is_null = &nulls[1];
        row[2].buffer_type = MYSQL_TYPE_STRING;   row[2].length = &lengths[2]; row[2].is_null = &nulls[2];
        row[3].buffer_type = MYSQL_TYPE_LONGLONG; row[3].buffer = &unused; row[3].is_null = &nulls[3];
        row[4].buffer_type = MYSQL_TYPE_LONGLONG; row[4].buffer = &expiresAt; row[4].is_null = &nulls[4];
        if (mysql_stmt_bind_result(lockToken.handle(), row) != 0) {
            lockToken.fail("bind_result");
        }
        const int fetched = mysql_stmt_fetch(lockToken.handle());
        if (fetched == 1) {
            lockToken.fail("fetch");
        }
        if (fetched == MYSQL_NO_DATA) {
            return result;  // unknown token
        }
        username = lockToken.fetchString(1, lengths[1], nulls[1]).value_or("");
        status = lockToken.fetchString(2, lengths[2], nulls[2]).value_or("");
    }
    if (unused != 1 || now >= expiresAt || status != "pending") {
        return result;  // used, expired or not pending: rolled back, nothing changes
    }

    // 2. Token used, account active.
    MariaDbParams useParams;
    useParams.integer(now).text(tokenHash);
    run(connection, kUseTokenSql, useParams, "use token");
    MariaDbParams activateParams;
    activateParams.integer(userID);
    run(connection, kActivateUserSql, activateParams, "activate user");

    // 3. The initial allotment (HU-08).
    std::vector<long long> objectIDs;
    {
        MariaDbStatement lockObjects(connection, kLockFreeObjectsSql);
        MariaDbParams limit;
        limit.integer(objects);
        if (!limit.execute(lockObjects)) {
            lockObjects.fail("lock free objects");
        }
        long long objectID = 0;
        MYSQL_BIND column{};
        column.buffer_type = MYSQL_TYPE_LONGLONG;
        column.buffer = &objectID;
        if (mysql_stmt_bind_result(lockObjects.handle(), &column) != 0) {
            lockObjects.fail("bind_result");
        }
        while (true) {
            const int status = mysql_stmt_fetch(lockObjects.handle());
            if (status == MYSQL_NO_DATA) break;
            if (status == 1) lockObjects.fail("fetch");
            objectIDs.push_back(objectID);
        }
    }
    for (const long long objectID : objectIDs) {
        MariaDbParams assign;
        assign.integer(userID).integer(now).integer(objectID);
        run(connection, kAssignObjectSql, assign, "assign object");
        MariaDbParams allocation;
        allocation.integer(objectID).integer(userID).integer(now);
        run(connection, kInsertAllocationSql, allocation, "record allocation");
    }

    // 4. The audit log, in the same transaction.
    MariaDbParams audit;
    audit.text(clientIP).text("cuenta").text("Cuenta activada con dotacion inicial").integer(userID)
        .text("objetos=" + std::to_string(objectIDs.size()));
    run(connection, kInsertAuditSql, audit, "insert audit log");
    if (static_cast<int>(objectIDs.size()) < objects) {
        // RF-2.13: the inventory ran short; the user gets what was left.
        MariaDbParams shortage;
        shortage.text(clientIP).text("cuenta").text("Inventario inicial insuficiente para la dotacion")
            .integer(userID).text("asignados=" + std::to_string(objectIDs.size()));
        run(connection, kInsertAuditSql, shortage, "insert audit log");
    }

    transaction.commit();
    result.activated = true;
    result.userID = static_cast<int>(userID);
    result.username = username;
    result.objectsAssigned = static_cast<int>(objectIDs.size());
    return result;
}

std::optional<PendingAccount> MariaDbAccountRepository::findPendingAccount(const std::string& email)
{
    MariaDbStatement statement(connection, kFindPendingSql);
    MariaDbParams params;
    params.text(email);
    if (!params.execute(statement)) {
        statement.fail("find pending account");
    }
    long long userID = 0, latest = 0;
    unsigned long lengths[4]{};
    my_bool nulls[4]{};
    MYSQL_BIND row[4]{};
    row[0].buffer_type = MYSQL_TYPE_LONGLONG; row[0].buffer = &userID; row[0].is_null = &nulls[0];
    row[1].buffer_type = MYSQL_TYPE_STRING;   row[1].length = &lengths[1]; row[1].is_null = &nulls[1];
    row[2].buffer_type = MYSQL_TYPE_STRING;   row[2].length = &lengths[2]; row[2].is_null = &nulls[2];
    row[3].buffer_type = MYSQL_TYPE_LONGLONG; row[3].buffer = &latest; row[3].is_null = &nulls[3];
    if (mysql_stmt_bind_result(statement.handle(), row) != 0) {
        statement.fail("bind_result");
    }
    const int fetched = mysql_stmt_fetch(statement.handle());
    if (fetched == MYSQL_NO_DATA) {
        return std::nullopt;
    }
    if (fetched == 1) {
        statement.fail("fetch");
    }
    PendingAccount account;
    account.userID = static_cast<int>(userID);
    account.username = statement.fetchString(1, lengths[1], nulls[1]).value_or("");
    account.email = statement.fetchString(2, lengths[2], nulls[2]).value_or("");
    if (!nulls[3]) {
        account.latestTokenExpiresAt = latest;
    }
    return account;
}

void MariaDbAccountRepository::replaceVerificationToken(int userID, const std::string& tokenHash,
                                                        std::int64_t expiresAt, std::int64_t now)
{
    MariaDbTransaction transaction(connection);
    MariaDbParams invalidate;
    invalidate.integer(now).integer(userID);
    run(connection, kInvalidateTokensSql, invalidate, "invalidate tokens");
    MariaDbParams insert;
    insert.text(tokenHash).integer(userID).integer(expiresAt);
    run(connection, kInsertTokenSql, insert, "insert verification token");
    transaction.commit();
}

void MariaDbAccountRepository::recordIncident(std::string_view category,
                                              std::string_view description,
                                              std::string_view clientIP, std::string_view details)
{
    MariaDbParams params;
    params.text(clientIP).text(category).text(description).text(details);
    run(connection, kInsertIncidentSql, params, "insert incident");
}

void MariaDbAccountRepository::recordEvent(std::string_view category, std::string_view description,
                                           std::string_view clientIP, std::optional<int> userID,
                                           std::string_view details)
{
    MariaDbParams params;
    params.text(clientIP).text(category).text(description);
    if (userID) {
        params.integer(*userID);
    } else {
        params.null();
    }
    params.text(details);
    run(connection, kInsertAuditSql, params, "insert audit log");
}
