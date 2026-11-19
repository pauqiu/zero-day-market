#include "infrastructure/crypto/aes_gcm_secret_box.h"

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

#include <fstream>
#include <memory>

#include "infrastructure/crypto/crypto_error.h"
#include "infrastructure/crypto/hex.h"

namespace {

constexpr unsigned char kVersion = 0x01;
constexpr std::size_t kNonceBytes = 12;
constexpr std::size_t kTagBytes = 16;

using CipherContext = std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)>;

CipherContext newContext()
{
    CipherContext context(EVP_CIPHER_CTX_new(), &EVP_CIPHER_CTX_free);
    if (!context) {
        throw CryptoError("cannot create cipher context");
    }
    return context;
}

}  // namespace

AesGcmSecretBox AesGcmSecretBox::fromKeyFile(const std::string& path)
{
    std::ifstream file(path);
    if (!file) {
        throw CryptoError("cannot open master key file: " + path);
    }
    std::string text;
    std::getline(file, text);
    while (!text.empty() && (text.back() == '\r' || text.back() == ' ')) {
        text.pop_back();
    }
    auto bytes = hexDecode(text);
    OPENSSL_cleanse(text.data(), text.size());
    if (!bytes || bytes->size() != 32) {
        throw CryptoError("master key file must contain 64 hexadecimal characters");
    }
    std::array<unsigned char, 32> key{};
    std::copy(bytes->begin(), bytes->end(), key.begin());
    OPENSSL_cleanse(bytes->data(), bytes->size());
    AesGcmSecretBox box(key);
    OPENSSL_cleanse(key.data(), key.size());
    return box;
}

AesGcmSecretBox AesGcmSecretBox::fromKey(const std::array<unsigned char, 32>& key)
{
    return AesGcmSecretBox(key);
}

AesGcmSecretBox::AesGcmSecretBox(const std::array<unsigned char, 32>& key) : key(key)
{
}

AesGcmSecretBox::AesGcmSecretBox(AesGcmSecretBox&& other) noexcept : key(other.key)
{
    OPENSSL_cleanse(other.key.data(), other.key.size());
}

AesGcmSecretBox::~AesGcmSecretBox()
{
    OPENSSL_cleanse(key.data(), key.size());
}

std::vector<unsigned char> AesGcmSecretBox::seal(const std::vector<unsigned char>& plaintext,
                                                 std::string_view context)
{
    std::vector<unsigned char> out(1 + kNonceBytes + plaintext.size() + kTagBytes);
    out[0] = kVersion;
    unsigned char* nonce = out.data() + 1;
    unsigned char* ciphertext = nonce + kNonceBytes;
    unsigned char* tag = ciphertext + plaintext.size();
    if (RAND_bytes(nonce, static_cast<int>(kNonceBytes)) != 1) {
        throw CryptoError("random generator failed");
    }

    CipherContext ctx = newContext();
    int length = 0;
    if (EVP_EncryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, key.data(), nonce) != 1 ||
        EVP_EncryptUpdate(ctx.get(), nullptr, &length,
                          reinterpret_cast<const unsigned char*>(context.data()),
                          static_cast<int>(context.size())) != 1 ||
        EVP_EncryptUpdate(ctx.get(), ciphertext, &length, plaintext.data(),
                          static_cast<int>(plaintext.size())) != 1 ||
        EVP_EncryptFinal_ex(ctx.get(), ciphertext + length, &length) != 1 ||
        EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_GET_TAG, static_cast<int>(kTagBytes), tag) != 1) {
        throw CryptoError("AES-GCM encryption failed");
    }
    return out;
}

std::optional<std::vector<unsigned char>> AesGcmSecretBox::open(
    const std::vector<unsigned char>& sealed, std::string_view context)
{
    if (sealed.size() < 1 + kNonceBytes + kTagBytes || sealed[0] != kVersion) {
        return std::nullopt;
    }
    const unsigned char* nonce = sealed.data() + 1;
    const unsigned char* ciphertext = nonce + kNonceBytes;
    const std::size_t ciphertextSize = sealed.size() - 1 - kNonceBytes - kTagBytes;
    unsigned char tag[kTagBytes];
    std::copy(ciphertext + ciphertextSize, ciphertext + ciphertextSize + kTagBytes, tag);

    std::vector<unsigned char> plaintext(ciphertextSize);
    CipherContext ctx = newContext();
    int length = 0;
    if (EVP_DecryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, key.data(), nonce) != 1 ||
        EVP_DecryptUpdate(ctx.get(), nullptr, &length,
                          reinterpret_cast<const unsigned char*>(context.data()),
                          static_cast<int>(context.size())) != 1 ||
        EVP_DecryptUpdate(ctx.get(), plaintext.data(), &length, ciphertext,
                          static_cast<int>(ciphertextSize)) != 1 ||
        EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_TAG, static_cast<int>(kTagBytes), tag) != 1) {
        throw CryptoError("AES-GCM decryption failed");
    }
    // The tag check happens here: altered data, another context or another key fail.
    if (EVP_DecryptFinal_ex(ctx.get(), plaintext.data() + length, &length) != 1) {
        OPENSSL_cleanse(plaintext.data(), plaintext.size());
        return std::nullopt;
    }
    return plaintext;
}
