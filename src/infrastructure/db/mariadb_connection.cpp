#include "infrastructure/db/mariadb_connection.h"

#include <cstring>
#include <string>

#include "infrastructure/db/database_error.h"

MariaDbConnection::MariaDbConnection(const DbConfig& config)
{
    mysql = mysql_init(nullptr);
    if (mysql == nullptr) {
        throw DatabaseError("mysql_init failed");
    }

    // Fail fast instead of hanging the request if VMDatos is unreachable.
    unsigned int connectTimeout = 3;
    unsigned int ioTimeout = 5;
    mysql_options(mysql, MYSQL_OPT_CONNECT_TIMEOUT, &connectTimeout);
    mysql_options(mysql, MYSQL_OPT_READ_TIMEOUT, &ioTimeout);
    mysql_options(mysql, MYSQL_OPT_WRITE_TIMEOUT, &ioTimeout);

    // The client never reads local files on behalf of the server.
    unsigned int localInfile = 0;
    mysql_options(mysql, MYSQL_OPT_LOCAL_INFILE, &localInfile);
    mysql_options(mysql, MYSQL_SET_CHARSET_NAME, "utf8mb4");

    // Always TLS (the server has require_secure_transport = ON), and refuse
    // to continue if the server's certificate is not signed by our CA or does
    // not match the host we connect to.
    my_bool enforceTls = 1;
    my_bool verifyServer = 1;
    mysql_options(mysql, MYSQL_OPT_SSL_ENFORCE, &enforceTls);
    mysql_options(mysql, MYSQL_OPT_SSL_CA, config.sslCa.c_str());
    mysql_options(mysql, MYSQL_OPT_SSL_VERIFY_SERVER_CERT, &verifyServer);

    if (mysql_real_connect(mysql, config.host.c_str(), config.user.c_str(),
                           config.password.c_str(), config.database.c_str(),
                           config.port, nullptr, 0) == nullptr) {
        // mysql_error never includes the password.
        const std::string error = mysql_error(mysql);
        mysql_close(mysql);
        mysql = nullptr;
        throw DatabaseError("cannot connect to database: " + error);
    }
}

MariaDbConnection::~MariaDbConnection()
{
    if (mysql != nullptr) {
        mysql_close(mysql);
    }
}

MariaDbStatement::MariaDbStatement(MariaDbConnection& connection, const char* sql)
{
    stmt = mysql_stmt_init(connection.handle());
    if (stmt == nullptr) {
        throw DatabaseError("mysql_stmt_init failed");
    }
    if (mysql_stmt_prepare(stmt, sql, static_cast<unsigned long>(std::strlen(sql))) != 0) {
        fail("prepare");
    }
}

MariaDbStatement::~MariaDbStatement()
{
    if (stmt != nullptr) {
        mysql_stmt_close(stmt);
    }
}

void MariaDbStatement::fail(const char* what)
{
    throw DatabaseError(std::string("statement ") + what + " failed: " +
                        mysql_stmt_error(stmt));
}

std::optional<std::string> MariaDbStatement::fetchString(unsigned int column, unsigned long length,
                                                        my_bool isNull)
{
    if (isNull) {
        return std::nullopt;
    }
    std::string value(length, '\0');
    if (length > 0) {
        MYSQL_BIND bind{};
        bind.buffer_type = MYSQL_TYPE_STRING;
        bind.buffer = value.data();
        bind.buffer_length = length;
        if (mysql_stmt_fetch_column(stmt, &bind, column, 0) != 0) {
            fail("fetch_column");
        }
    }
    return value;
}

MariaDbParams& MariaDbParams::text(std::string_view value)
{
    buffers.emplace_back(value);
    lengths.push_back(static_cast<unsigned long>(value.size()));
    MYSQL_BIND bind{};
    bind.buffer_type = MYSQL_TYPE_STRING;
    bind.buffer = buffers.back().data();
    bind.buffer_length = lengths.back();
    bind.length = &lengths.back();
    binds.push_back(bind);
    return *this;
}

MariaDbParams& MariaDbParams::blob(const std::vector<unsigned char>& value)
{
    buffers.emplace_back(value.begin(), value.end());
    lengths.push_back(static_cast<unsigned long>(value.size()));
    MYSQL_BIND bind{};
    bind.buffer_type = MYSQL_TYPE_BLOB;
    bind.buffer = buffers.back().data();
    bind.buffer_length = lengths.back();
    bind.length = &lengths.back();
    binds.push_back(bind);
    return *this;
}

MariaDbParams& MariaDbParams::integer(long long value)
{
    integers.push_back(value);
    MYSQL_BIND bind{};
    bind.buffer_type = MYSQL_TYPE_LONGLONG;
    bind.buffer = &integers.back();
    binds.push_back(bind);
    return *this;
}

MariaDbParams& MariaDbParams::null()
{
    MYSQL_BIND bind{};
    bind.buffer_type = MYSQL_TYPE_NULL;
    bind.is_null = &isNull;
    binds.push_back(bind);
    return *this;
}

bool MariaDbParams::execute(MariaDbStatement& statement)
{
    if (mysql_stmt_param_count(statement.handle()) != binds.size()) {
        throw DatabaseError("statement parameter count does not match");
    }
    if (!binds.empty() && mysql_stmt_bind_param(statement.handle(), binds.data()) != 0) {
        statement.fail("bind_param");
    }
    return mysql_stmt_execute(statement.handle()) == 0;
}

MariaDbTransaction::MariaDbTransaction(MariaDbConnection& connection) : mysql(connection.handle())
{
    if (mysql_autocommit(mysql, 0) != 0) {
        throw DatabaseError(std::string("cannot start transaction: ") + mysql_error(mysql));
    }
}

MariaDbTransaction::~MariaDbTransaction()
{
    if (!finished) {
        mysql_rollback(mysql);
    }
    mysql_autocommit(mysql, 1);
}

void MariaDbTransaction::commit()
{
    if (mysql_commit(mysql) != 0) {
        throw DatabaseError(std::string("commit failed: ") + mysql_error(mysql));
    }
    finished = true;
}
