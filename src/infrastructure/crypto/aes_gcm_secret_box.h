#ifndef AES_GCM_SECRET_BOX_H
#define AES_GCM_SECRET_BOX_H

#include <array>
#include <string>

#include "application/accounts/secret_box.h"

// AES-256-GCM with a 32-byte master key read from a file outside the web
// root and outside the database (/etc/jesterschest, root:apache 0640).
// Sealed format: 0x01 (version) | 12-byte random nonce | ciphertext | 16-byte tag.
// The context is authenticated data: it is not stored, but it must match on open.
class AesGcmSecretBox : public SecretBox {
    public:
        // The file holds the key as 64 hexadecimal characters. Throws CryptoError.
        static AesGcmSecretBox fromKeyFile(const std::string& path);
        static AesGcmSecretBox fromKey(const std::array<unsigned char, 32>& key);

        ~AesGcmSecretBox() override;
        AesGcmSecretBox(const AesGcmSecretBox&) = delete;
        AesGcmSecretBox& operator=(const AesGcmSecretBox&) = delete;
        AesGcmSecretBox(AesGcmSecretBox&& other) noexcept;
        AesGcmSecretBox& operator=(AesGcmSecretBox&&) = delete;

        std::vector<unsigned char> seal(const std::vector<unsigned char>& plaintext,
                                        std::string_view context) override;
        std::optional<std::vector<unsigned char>> open(const std::vector<unsigned char>& sealed,
                                                       std::string_view context) override;

    private:
        explicit AesGcmSecretBox(const std::array<unsigned char, 32>& key);

        std::array<unsigned char, 32> key{};
};

#endif // AES_GCM_SECRET_BOX_H
