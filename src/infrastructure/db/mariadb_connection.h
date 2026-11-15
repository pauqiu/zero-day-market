#ifndef MARIADB_CONNECTION_H
#define MARIADB_CONNECTION_H

#include <mysql.h>

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

    private:
        MYSQL_STMT* stmt = nullptr;
};

#endif // MARIADB_CONNECTION_H
