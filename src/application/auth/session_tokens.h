#ifndef SESSION_TOKENS_H
#define SESSION_TOKENS_H

#include <string>
#include <string_view>

// Port: session identifiers (control: row 3).
class SessionTokens {
    public:
        virtual ~SessionTokens() = default;

        // 256 random bits from a cryptographic generator, as 64 hex characters.
        virtual std::string newToken() = 0;

        // SHA-256 of the token, as 64 hex characters. Only this is stored.
        virtual std::string hash(std::string_view token) = 0;
};

#endif // SESSION_TOKENS_H
