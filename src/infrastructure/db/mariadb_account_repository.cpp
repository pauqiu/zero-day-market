#include "infrastructure/db/mariadb_account_repository.h"

#include "infrastructure/db/database_error.h"

namespace {

constexpr unsigned int kDuplicateEntry = 1062;  // ER_DUP_ENTRY

// TEMPORARY: accounts are created active until email verification (HU-07)
// exists. Then the initial status becomes 'pending'.
constexpr const char* kInsertUserSql =
    "INSERT INTO users (username, email, password_hash, status) VALUES (?, ?, ?, 'active')";

constexpr const char* kInsertYubikeySql =
    "INSERT INTO yubikeys (user_id, public_id, aes_key_enc, private_id_enc, "
    "                      last_counter, last_session_use, registered_at) "
    "VALUES (?, ?, ?, ?, ?, ?, UTC_TIMESTAMP())";

constexpr const char* kInsertAuditSql =
    "INSERT INTO audit_log (created_at, ip, category, description, user_id, details) "
    "VALUES (UTC_TIMESTAMP(3), ?, 'cuenta', 'Registro de usuario con YubiKey', ?, ?)";

constexpr const char* kInsertIncidentSql =
    "INSERT INTO security_incidents (created_at, ip, category, description, details) "
    "VALUES (UTC_TIMESTAMP(3), ?, ?, ?, ?)";

// Which unique constraint a duplicate-key error refers to. The constraint
// names come from schema.sql; the message never reaches the user.
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

    MariaDbStatement insertAudit(connection, kInsertAuditSql);
    MariaDbParams auditParams;
    auditParams.text(account.clientIP).integer(userID).text("public_id=" + account.publicID);
    if (!auditParams.execute(insertAudit)) {
        insertAudit.fail("insert audit log");
    }

    transaction.commit();
    result.outcome = CreateAccountOutcome::Created;
    result.userID = static_cast<int>(userID);
    return result;
}

void MariaDbAccountRepository::recordIncident(std::string_view category,
                                              std::string_view description,
                                              std::string_view clientIP, std::string_view details)
{
    MariaDbStatement statement(connection, kInsertIncidentSql);
    MariaDbParams params;
    params.text(clientIP).text(category).text(description).text(details);
    if (!params.execute(statement)) {
        statement.fail("insert incident");
    }
}
