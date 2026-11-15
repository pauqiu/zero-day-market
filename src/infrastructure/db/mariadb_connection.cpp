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
