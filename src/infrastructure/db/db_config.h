#ifndef DB_CONFIG_H
#define DB_CONFIG_H

#include <string>

struct DbConfig {
    std::string host;
    unsigned int port = 3306;
    std::string user;
    std::string password;
    std::string database;
    // CA certificate that signed the database server's certificate. The
    // connection always uses TLS and verifies the server against it.
    std::string sslCa;
};

// Reads a "key = value" file (see deploy/db.conf.example). The file lives
// outside the web root and is readable only by root and the apache group.
// Throws DatabaseError if the file is missing or incomplete.
DbConfig loadDbConfig(const std::string& path);

#endif // DB_CONFIG_H
