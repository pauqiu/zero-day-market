#include "infrastructure/crypto/scrypt_password_hasher.h"

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

#include <cstdint>
#include <vector>

#include "infrastructure/crypto/crypto_error.h"
#include "infrastructure/crypto/hex.h"

namespace {

constexpr std::size_t kSaltBytes = 16;
constexpr std::size_t kHashBytes = 32;

// Accepted ranges when reading a stored hash: a tampered row cannot make the
// server spend unbounded memory or time.
bool parametersInRange(int log2N, int r, int p)
{
    return log2N >= 10 && log2N <= 20 && r >= 1 && r <= 16 && p >= 1 && p <= 4;
}

std::vector<unsigned char> derive(std::string_view password, const std::vector<unsigned char>& salt,
                                  int log2N, int r, int p)
{
    const std::uint64_t N = std::uint64_t{1} << log2N;
    // scrypt needs about 128 * r * N bytes (plus 128 * r * p); allow twice that.
    const std::uint64_t maxMemory = 2 * 128 * static_cast<std::uint64_t>(r) * (N + p);
    std::vector<unsigned char> out(kHashBytes);
    if (EVP_PBE_scrypt(password.data(), password.size(), salt.data(), salt.size(), N,
                       static_cast<std::uint64_t>(r), static_cast<std::uint64_t>(p), maxMemory,
                       out.data(), out.size()) != 1) {
        throw CryptoError("scrypt derivation failed");
    }
    return out;
}

// Splits "a$b$c" into its fields.
std::vector<std::string_view> split(std::string_view text, char separator)
{
    std::vector<std::string_view> fields;
    std::size_t start = 0;
    while (true) {
        const std::size_t end = text.find(separator, start);
        fields.push_back(text.substr(start, end - start));
        if (end == std::string_view::npos) {
            return fields;
        }
        start = end + 1;
    }
}

std::optional<int> smallInt(std::string_view text)
{
    if (text.empty() || text.size() > 3) {
        return std::nullopt;
    }
    int value = 0;
    for (const char c : text) {
        if (c < '0' || c > '9') {
            return std::nullopt;
        }
        value = value * 10 + (c - '0');
    }
    return value;
}

}  // namespace

ScryptPasswordHasher::ScryptPasswordHasher() : ScryptPasswordHasher(Parameters{})
{
}

ScryptPasswordHasher::ScryptPasswordHasher(Parameters parameters)
    : parameters(parameters)
{
    if (!parametersInRange(parameters.log2N, parameters.r, parameters.p)) {
        throw CryptoError("scrypt parameters out of range");
    }
}

std::string ScryptPasswordHasher::hash(std::string_view password)
{
    std::vector<unsigned char> salt(kSaltBytes);
    if (RAND_bytes(salt.data(), static_cast<int>(salt.size())) != 1) {
        throw CryptoError("random generator failed");
    }
    std::vector<unsigned char> derived =
        derive(password, salt, parameters.log2N, parameters.r, parameters.p);
    std::string stored = "scrypt$" + std::to_string(parameters.log2N) + "$" +
                         std::to_string(parameters.r) + "$" + std::to_string(parameters.p) + "$" +
                         hexEncode(salt.data(), salt.size()) + "$" +
                         hexEncode(derived.data(), derived.size());
    OPENSSL_cleanse(derived.data(), derived.size());
    return stored;
}

bool ScryptPasswordHasher::verify(std::string_view password, std::string_view stored)
{
    const std::vector<std::string_view> fields = split(stored, '$');
    if (fields.size() != 6 || fields[0] != "scrypt") {
        return false;
    }
    const auto log2N = smallInt(fields[1]);
    const auto r = smallInt(fields[2]);
    const auto p = smallInt(fields[3]);
    const auto salt = hexDecode(fields[4]);
    const auto expected = hexDecode(fields[5]);
    if (!log2N || !r || !p || !salt || !expected || !parametersInRange(*log2N, *r, *p) ||
        salt->size() != kSaltBytes || expected->size() != kHashBytes) {
        return false;
    }
    std::vector<unsigned char> derived = derive(password, *salt, *log2N, *r, *p);
    // Constant-time comparison: the time taken does not reveal how many bytes matched.
    const bool equal = CRYPTO_memcmp(derived.data(), expected->data(), kHashBytes) == 0;
    OPENSSL_cleanse(derived.data(), derived.size());
    return equal;
}
