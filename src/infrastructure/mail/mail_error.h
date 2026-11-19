#ifndef MAIL_ERROR_H
#define MAIL_ERROR_H

#include <stdexcept>
#include <string>

class MailError : public std::runtime_error {
    public:
        explicit MailError(const std::string& message) : std::runtime_error(message) {}
};

#endif // MAIL_ERROR_H
