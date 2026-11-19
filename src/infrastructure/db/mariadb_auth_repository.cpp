#include "infrastructure/db/mariadb_auth_repository.h"

#include "infrastructure/db/database_error.h"

namespace {

// Dates are stored in UTC and exchanged as epoch seconds without depending on
// the session time zone: TIMESTAMPDIFF from the epoch to read them, and
// TIMESTAMPADD from the epoch to write them.
#define EPOCH_OF(column) "TIMESTAMPDIFF(SECOND, '1970-01-01 00:00:00', " column ")"
#define FROM_EPOCH "TIMESTAMPADD(SECOND, ?, '1970-01-01 00:00:00')"

constexpr const char* kUserColumns =
    "SELECT user_id, username, email, password_hash, status, " EPOCH_OF("locked_until") " FROM users ";

const std::string kFindByUsernameSql = std::string(kUserColumns) + "WHERE username = ?";
const std::string kFindByEmailSql = std::string(kUserColumns) + "WHERE email = ?";
const std::string kFindByIDSql = std::string(kUserColumns) + "WHERE user_id = ?";

// MariaDB applies the assignments of an UPDATE from left to right, so
// locked_until is computed before failed_attempts changes. Reaching the
// limit locks the account and restarts the counter; all in one statement,
// so simultaneous failures are all counted.
constexpr const char* kFailedAttemptSql =
    "UPDATE users SET "
    "  locked_until = CASE WHEN failed_attempts + 1 >= ? THEN " FROM_EPOCH " ELSE locked_until END, "
    "  failed_attempts = CASE WHEN failed_attempts + 1 >= ? THEN 0 ELSE failed_attempts + 1 END "
    "WHERE user_id = ?";

constexpr const char* kLockedUntilIsSql =
    "SELECT COUNT(*) FROM users WHERE user_id = ? AND locked_until = " FROM_EPOCH;

constexpr const char* kResetAttemptsSql =
    "UPDATE users SET failed_attempts = 0, locked_until = NULL WHERE user_id = ?";

constexpr const char* kFindYubikeySql =
    "SELECT key_id, public_id, aes_key_enc, private_id_enc, last_counter, last_session_use "
    "FROM yubikeys WHERE user_id = ?";

// Only moves forward: the WHERE repeats the "newer OTP" check, so if two
// requests use the same OTP at the same time, only one updates the row.
constexpr const char* kAdvanceCountersSql =
    "UPDATE yubikeys SET last_counter = ?, last_session_use = ? "
    "WHERE key_id = ? AND (last_counter < ? OR (last_counter = ? AND last_session_use < ?))";

constexpr const char* kCreateSessionSql =
    "INSERT INTO sessions (session_id_hash, user_id, ip, csrf_token, mfa_verified, "
    "                      created_at, last_seen_at, expires_at) "
    "VALUES (?, ?, ?, '', ?, " FROM_EPOCH ", " FROM_EPOCH ", " FROM_EPOCH ")";

constexpr const char* kFindSessionSql =
    "SELECT s.user_id, u.username, r.name = 'admin', u.status = 'active', s.mfa_verified, "
    "       " EPOCH_OF("s.created_at") ", " EPOCH_OF("s.last_seen_at") ", " EPOCH_OF("s.expires_at") " "
    "FROM sessions s "
    "JOIN users u ON u.user_id = s.user_id "
    "JOIN roles r ON r.role_id = u.role_id "
    "WHERE s.session_id_hash = ?";

constexpr const char* kTouchSessionSql =
    "UPDATE sessions SET last_seen_at = " FROM_EPOCH " WHERE session_id_hash = ?";

constexpr const char* kDeleteSessionSql = "DELETE FROM sessions WHERE session_id_hash = ?";

constexpr const char* kAuditSql =
    "INSERT INTO audit_log (created_at, ip, category, description, user_id, details) "
    "VALUES (UTC_TIMESTAMP(3), ?, ?, ?, ?, ?)";

constexpr const char* kIncidentSql =
    "INSERT INTO security_incidents (created_at, ip, category, description, user_id, details) "
    "VALUES (UTC_TIMESTAMP(3), ?, ?, ?, ?, ?)";

// Binds string/int/blob result columns, all fetched with zero-size string
// buffers first and read afterwards with fetchString.
struct Row {
    explicit Row(unsigned int columns) : lengths(columns), nulls(columns), binds(columns), ints(columns) {}

    void asString(unsigned int column)
    {
        binds[column] = MYSQL_BIND{};
        binds[column].buffer_type = MYSQL_TYPE_STRING;
        binds[column].length = &lengths[column];
        binds[column].is_null = &nulls[column];
    }

    void asInteger(unsigned int column)
    {
        binds[column] = MYSQL_BIND{};
        binds[column].buffer_type = MYSQL_TYPE_LONGLONG;
        binds[column].buffer = &ints[column];
        binds[column].is_null = &nulls[column];
    }

    std::vector<unsigned long> lengths;
    std::vector<my_bool> nulls;
    std::vector<MYSQL_BIND> binds;
    std::vector<long long> ints;
};

// Executes and fetches at most one row. Returns false if there was none.
bool fetchOne(MariaDbStatement& statement, MariaDbParams& params, Row& row)
{
    if (!params.execute(statement)) {
        statement.fail("execute");
    }
    if (mysql_stmt_bind_result(statement.handle(), row.binds.data()) != 0) {
        statement.fail("bind_result");
    }
    const int status = mysql_stmt_fetch(statement.handle());
    if (status == MYSQL_NO_DATA) {
        return false;
    }
    if (status == 1) {
        statement.fail("fetch");
    }
    return true;
}

std::vector<unsigned char> toBytes(const std::optional<std::string>& value)
{
    return value ? std::vector<unsigned char>(value->begin(), value->end())
                 : std::vector<unsigned char>{};
}

}  // namespace

MariaDbAuthRepository::MariaDbAuthRepository(MariaDbConnection& connection)
    : connection(connection)
{
}

void MariaDbAuthRepository::execute(const char* sql, MariaDbParams& params, const char* what)
{
    MariaDbStatement statement(connection, sql);
    if (!params.execute(statement)) {
        statement.fail(what);
    }
}

std::optional<LoginUser> MariaDbAuthRepository::findUser(const char* sql, MariaDbParams& params)
{
    MariaDbStatement statement(connection, sql);
    Row row(6);
    row.asInteger(0);
    row.asString(1);
    row.asString(2);
    row.asString(3);
    row.asString(4);
    row.asInteger(5);
    if (!fetchOne(statement, params, row)) {
        return std::nullopt;
    }
    LoginUser user;
    user.userID = static_cast<int>(row.ints[0]);
    user.username = statement.fetchString(1, row.lengths[1], row.nulls[1]).value_or("");
    user.email = statement.fetchString(2, row.lengths[2], row.nulls[2]).value_or("");
    user.passwordHash = statement.fetchString(3, row.lengths[3], row.nulls[3]).value_or("");
    user.status = statement.fetchString(4, row.lengths[4], row.nulls[4]).value_or("");
    if (!row.nulls[5]) {
        user.lockedUntil = row.ints[5];
    }
    return user;
}

std::optional<LoginUser> MariaDbAuthRepository::findUserByUsername(const std::string& username)
{
    MariaDbParams params;
    params.text(username);
    return findUser(kFindByUsernameSql.c_str(), params);
}

std::optional<LoginUser> MariaDbAuthRepository::findUserByEmail(const std::string& email)
{
    MariaDbParams params;
    params.text(email);
    return findUser(kFindByEmailSql.c_str(), params);
}

std::optional<LoginUser> MariaDbAuthRepository::findUserByID(int userID)
{
    MariaDbParams params;
    params.integer(userID);
    return findUser(kFindByIDSql.c_str(), params);
}

bool MariaDbAuthRepository::recordFailedAttempt(int userID, int maxAttempts, std::int64_t lockUntil)
{
    MariaDbParams update;
    update.integer(maxAttempts).integer(lockUntil).integer(maxAttempts).integer(userID);
    execute(kFailedAttemptSql, update, "record failed attempt");

    // Locked by THIS attempt if locked_until is exactly the value we set.
    MariaDbStatement check(connection, kLockedUntilIsSql);
    MariaDbParams params;
    params.integer(userID).integer(lockUntil);
    Row row(1);
    row.asInteger(0);
    return fetchOne(check, params, row) && row.ints[0] == 1;
}

void MariaDbAuthRepository::resetFailedAttempts(int userID)
{
    MariaDbParams params;
    params.integer(userID);
    execute(kResetAttemptsSql, params, "reset failed attempts");
}

std::optional<StoredYubikey> MariaDbAuthRepository::findYubikey(int userID)
{
    MariaDbStatement statement(connection, kFindYubikeySql);
    MariaDbParams params;
    params.integer(userID);
    Row row(6);
    row.asInteger(0);
    row.asString(1);
    row.asString(2);
    row.asString(3);
    row.asInteger(4);
    row.asInteger(5);
    if (!fetchOne(statement, params, row)) {
        return std::nullopt;
    }
    StoredYubikey key;
    key.keyID = static_cast<int>(row.ints[0]);
    key.publicID = statement.fetchString(1, row.lengths[1], row.nulls[1]).value_or("");
    key.aesKeySealed = toBytes(statement.fetchString(2, row.lengths[2], row.nulls[2]));
    key.privateIDSealed = toBytes(statement.fetchString(3, row.lengths[3], row.nulls[3]));
    key.lastCounter = static_cast<int>(row.ints[4]);
    key.lastSessionUse = static_cast<int>(row.ints[5]);
    return key;
}

bool MariaDbAuthRepository::advanceOtpCounters(int keyID, int counter, int sessionUse)
{
    MariaDbStatement statement(connection, kAdvanceCountersSql);
    MariaDbParams params;
    params.integer(counter).integer(sessionUse).integer(keyID)
          .integer(counter).integer(counter).integer(sessionUse);
    if (!params.execute(statement)) {
        statement.fail("advance OTP counters");
    }
    return mysql_stmt_affected_rows(statement.handle()) == 1;
}

void MariaDbAuthRepository::createSession(const NewSession& session)
{
    MariaDbParams params;
    params.text(session.idHash)
        .integer(session.userID)
        .text(session.clientIP)
        .integer(session.mfaVerified ? 1 : 0)
        .integer(session.createdAt)
        .integer(session.createdAt)
        .integer(session.expiresAt);
    execute(kCreateSessionSql, params, "create session");
}

std::optional<SessionRecord> MariaDbAuthRepository::findSession(const std::string& idHash)
{
    MariaDbStatement statement(connection, kFindSessionSql);
    MariaDbParams params;
    params.text(idHash);
    Row row(8);
    row.asInteger(0);
    row.asString(1);
    for (unsigned int column = 2; column < 8; ++column) {
        row.asInteger(column);
    }
    if (!fetchOne(statement, params, row)) {
        return std::nullopt;
    }
    SessionRecord session;
    session.idHash = idHash;
    session.userID = static_cast<int>(row.ints[0]);
    session.username = statement.fetchString(1, row.lengths[1], row.nulls[1]).value_or("");
    session.isAdmin = row.ints[2] == 1;
    session.userActive = row.ints[3] == 1;
    session.mfaVerified = row.ints[4] == 1;
    session.createdAt = row.ints[5];
    session.lastSeenAt = row.ints[6];
    session.expiresAt = row.ints[7];
    return session;
}

void MariaDbAuthRepository::touchSession(const std::string& idHash, std::int64_t now)
{
    MariaDbParams params;
    params.integer(now).text(idHash);
    execute(kTouchSessionSql, params, "touch session");
}

void MariaDbAuthRepository::deleteSession(const std::string& idHash)
{
    MariaDbParams params;
    params.text(idHash);
    execute(kDeleteSessionSql, params, "delete session");
}

void MariaDbAuthRepository::audit(std::string_view category, std::string_view description,
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
    execute(kAuditSql, params, "insert audit log");
}

void MariaDbAuthRepository::incident(std::string_view category, std::string_view description,
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
    execute(kIncidentSql, params, "insert incident");
}
