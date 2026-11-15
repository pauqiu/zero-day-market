#ifndef DATABASE_ERROR_H
#define DATABASE_ERROR_H

#include <stdexcept>
#include <string>

// Raised by the infrastructure layer. Its message is for the server log
// only; it must never be shown to the user (it may describe the schema).
class DatabaseError : public std::runtime_error {
    public:
        explicit DatabaseError(const std::string& message)
            : std::runtime_error(message) {}
};

#endif // DATABASE_ERROR_H
