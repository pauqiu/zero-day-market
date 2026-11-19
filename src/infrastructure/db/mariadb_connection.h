#ifndef MARIADB_CONNECTION_H
#define MARIADB_CONNECTION_H

#include <mysql.h>

#include <deque>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

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

        // Server error of the last failed call (for example 1062, duplicate key).
        unsigned int errorNumber() { return mysql_stmt_errno(stmt); }
        std::string errorMessage() { return mysql_stmt_error(stmt); }

    private:
        MYSQL_STMT* stmt = nullptr;
};

// Parameters for a prepared statement. Keeps a copy of every value, so the
// buffers stay valid until the statement runs. Values never become SQL text.
class MariaDbParams {
    public:
        MariaDbParams& text(std::string_view value);
        MariaDbParams& blob(const std::vector<unsigned char>& value);
        MariaDbParams& integer(long long value);
        MariaDbParams& null();

        // Binds the parameters and executes. Returns false if the server
        // rejected the statement (the error is then in the statement).
        bool execute(MariaDbStatement& statement);

    private:
        std::deque<std::string> buffers;
        std::deque<long long> integers;
        std::deque<unsigned long> lengths;
        std::vector<MYSQL_BIND> binds;
        my_bool isNull = 1;
};

// A transaction on the connection. If it is not committed (an error or an
// early return), the destructor rolls it back: all changes or none.
class MariaDbTransaction {
    public:
        explicit MariaDbTransaction(MariaDbConnection& connection);
        ~MariaDbTransaction();

        MariaDbTransaction(const MariaDbTransaction&) = delete;
        MariaDbTransaction& operator=(const MariaDbTransaction&) = delete;

        void commit();

    private:
        MYSQL* mysql;
        bool finished = false;
};

#endif // MARIADB_CONNECTION_H
