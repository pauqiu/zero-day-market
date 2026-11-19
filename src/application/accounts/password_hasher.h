#ifndef PASSWORD_HASHER_H
#define PASSWORD_HASHER_H

#include <string>
#include <string_view>

// Port: slow, salted password hashing (control: row 14 of the security
// architecture). The stored string carries its own parameters.
class PasswordHasher {
    public:
        virtual ~PasswordHasher() = default;

        virtual std::string hash(std::string_view password) = 0;

        virtual bool verify(std::string_view password, std::string_view stored) = 0;
};

#endif // PASSWORD_HASHER_H
