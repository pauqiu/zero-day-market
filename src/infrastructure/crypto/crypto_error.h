#ifndef CRYPTO_ERROR_H
#define CRYPTO_ERROR_H

#include <stdexcept>
#include <string>

// Raised when a cryptographic operation or key cannot be used. The message is
// for the server log and never contains secret material.
class CryptoError : public std::runtime_error {
    public:
        explicit CryptoError(const std::string& message) : std::runtime_error(message) {}
};

#endif // CRYPTO_ERROR_H
