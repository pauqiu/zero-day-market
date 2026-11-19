#ifndef MARIADB_CONNECTION_H
#define MARIADB_CONNECTION_H

#include <mysql.h>

#include <optional>
#include <string>

#include "infrastructure/db/db_config.h"

// Owns one connection for the life of the CGI process (one request).
class MariaDbConnection {
    public:
        explicit MariaDbConnection(const DbConfig& config);
        ~MariaDbConnection();

        MariaDbConnection(const MariaDbConnection&) = delete;
        MariaDbConnection& operator=(const MariaDbConnection&) = delete;

        MYSQL* handle() { return mysql; }

    private:
        MYSQL* mysql = nullptr;
};

// Owns one prepared statement; closes it on every exit path.
class MariaDbStatement {
    public:
        MariaDbStatement(MariaDbConnection& connection, const char* sql);
        ~MariaDbStatement();

        MariaDbStatement(const MariaDbStatement&) = delete;
        MariaDbStatement& operator=(const MariaDbStatement&) = delete;

        MYSQL_STMT* handle() { return stmt; }

        // Throws DatabaseError with the statement's last error.
        [[noreturn]] void fail(const char* what);

        // Reads a string column after mysql_stmt_fetch, when it was bound with
        // a zero-size buffer to learn its length first. Empty if SQL NULL.
        std::optional<std::string> fetchString(unsigned int column, unsigned long length,
                                               my_bool isNull);

    private:
        MYSQL_STMT* stmt = nullptr;
};

#endif // MARIADB_CONNECTION_H
