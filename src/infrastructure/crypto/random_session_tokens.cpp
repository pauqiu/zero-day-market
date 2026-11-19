#include "infrastructure/crypto/random_session_tokens.h"

#include <openssl/evp.h>
#include <openssl/rand.h>

#include "infrastructure/crypto/crypto_error.h"
#include "infrastructure/crypto/hex.h"

std::string RandomSessionTokens::newToken()
{
    unsigned char bytes[32];
    if (RAND_bytes(bytes, sizeof bytes) != 1) {
        throw CryptoError("random generator failed");
    }
    return hexEncode(bytes, sizeof bytes);
}

std::string RandomSessionTokens::hash(std::string_view token)
{
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int length = 0;
    if (EVP_Digest(token.data(), token.size(), digest, &length, EVP_sha256(), nullptr) != 1) {
        throw CryptoError("SHA-256 failed");
    }
    return hexEncode(digest, length);
}
