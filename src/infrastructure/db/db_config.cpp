#include "infrastructure/db/db_config.h"

#include <fstream>
#include <string>

#include "infrastructure/db/database_error.h"

namespace {

std::string trim(const std::string& text)
{
    const char* spaces = " \t\r\n";
    const auto first = text.find_first_not_of(spaces);
    if (first == std::string::npos) {
        return "";
    }
    const auto last = text.find_last_not_of(spaces);
    return text.substr(first, last - first + 1);
}

}  // namespace

DbConfig loadDbConfig(const std::string& path)
{
    std::ifstream file(path);
    if (!file) {
        throw DatabaseError("cannot open database configuration: " + path);
    }

    DbConfig config;
    std::string line;
    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') {
            continue;
        }
        const auto equals = line.find('=');
        if (equals == std::string::npos) {
            throw DatabaseError("malformed line in database configuration");
        }
        const std::string key = trim(line.substr(0, equals));
        const std::string value = trim(line.substr(equals + 1));

        if (key == "host") {
            config.host = value;
        } else if (key == "port") {
            try {
                config.port = static_cast<unsigned int>(std::stoul(value));
            } catch (const std::exception&) {
                throw DatabaseError("invalid port in database configuration");
            }
        } else if (key == "user") {
            config.user = value;
        } else if (key == "password") {
            config.password = value;
        } else if (key == "database") {
            config.database = value;
        }
        // Unknown keys are ignored so the file can grow without breaking.
    }

    if (config.host.empty() || config.user.empty() || config.database.empty()) {
        throw DatabaseError("database configuration needs host, user and database");
    }
    return config;
}
