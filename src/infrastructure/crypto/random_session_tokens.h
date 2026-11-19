#ifndef RANDOM_SESSION_TOKENS_H
#define RANDOM_SESSION_TOKENS_H

#include "application/auth/session_tokens.h"

// Session identifiers from OpenSSL's cryptographic generator (RAND_bytes),
// stored only as their SHA-256.
class RandomSessionTokens : public SessionTokens {
    public:
        std::string newToken() override;
        std::string hash(std::string_view token) override;
};

#endif // RANDOM_SESSION_TOKENS_H
