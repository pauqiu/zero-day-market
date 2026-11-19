#ifndef SCRYPT_PASSWORD_HASHER_H
#define SCRYPT_PASSWORD_HASHER_H

#include "application/accounts/password_hasher.h"

// scrypt (OpenSSL EVP_PBE_scrypt): slow and memory-hard, with a random
// 16-byte salt per password. Stored as
//     scrypt$<log2 N>$<r>$<p>$<salt hex>$<hash hex>
// so the parameters can be raised later without breaking existing hashes.
class ScryptPasswordHasher : public PasswordHasher {
    public:
        struct Parameters {
            int log2N = 15;  // N = 32768: about 32 MiB of memory per hash
            int r = 8;
            int p = 1;
        };

        ScryptPasswordHasher();  // production parameters
        explicit ScryptPasswordHasher(Parameters parameters);

        std::string hash(std::string_view password) override;
        bool verify(std::string_view password, std::string_view stored) override;

    private:
        Parameters parameters;
};

#endif // SCRYPT_PASSWORD_HASHER_H
