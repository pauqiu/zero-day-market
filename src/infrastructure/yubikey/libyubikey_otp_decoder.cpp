#include "infrastructure/yubikey/libyubikey_otp_decoder.h"

#include <openssl/crypto.h>
#include <yubikey.h>

#include <cstdint>
#include <cstring>

namespace {

constexpr std::size_t kPublicIDLength = 12;
constexpr std::size_t kOtpLength = kPublicIDLength + 2 * YUBIKEY_BLOCK_SIZE;  // 44
constexpr std::size_t kAesKeyHexLength = 2 * YUBIKEY_KEY_SIZE;               // 32

}  // namespace

std::optional<DecodedOtp> LibyubikeyOtpDecoder::decode(std::string_view otp,
                                                       std::string_view aesKeyHex)
{
    // libyubikey works on fixed-size C buffers and does not check lengths, so
    // the sizes and alphabets are verified here before any call into it.
    if (otp.size() != kOtpLength || aesKeyHex.size() != kAesKeyHexLength) {
        return std::nullopt;
    }
    char otpText[kOtpLength + 1];
    char keyText[kAesKeyHexLength + 1];
    std::memcpy(otpText, otp.data(), kOtpLength);
    otpText[kOtpLength] = '\0';
    std::memcpy(keyText, aesKeyHex.data(), kAesKeyHexLength);
    keyText[kAesKeyHexLength] = '\0';
    if (!yubikey_modhex_p(otpText) || !yubikey_hex_p(keyText)) {
        OPENSSL_cleanse(keyText, sizeof keyText);
        return std::nullopt;
    }

    // The AES key goes from hex to 16 bytes. Careful: despite what yubikey.h
    // suggests, the last argument is the size of the DESTINATION buffer: the
    // library writes exactly that many bytes (16 bytes plus a zero here).
    // Passing the length of the text (32) overflows the buffer.
    char key[YUBIKEY_KEY_SIZE + 1];
    yubikey_hex_decode(key, keyText, sizeof key);
    OPENSSL_cleanse(keyText, sizeof keyText);

    // Decrypt the block. Note: although yubikey.h declares the first argument
    // as 16 bytes, yubikey_parse takes the 32 MODHEX characters and decodes
    // them itself. It does NOT check the CRC: we do. With a wrong key the
    // block decrypts to random bytes, whose CRC matches 1 time in 65536.
    yubikey_token_st token;
    yubikey_parse(reinterpret_cast<const std::uint8_t*>(otpText + kPublicIDLength),
                  reinterpret_cast<const std::uint8_t*>(key), &token);
    const bool valid = yubikey_crc_ok_p(reinterpret_cast<std::uint8_t*>(&token));

    std::optional<DecodedOtp> result;
    if (valid) {
        DecodedOtp decoded;
        decoded.publicID.assign(otp.substr(0, kPublicIDLength));
        decoded.privateID.assign(token.uid, token.uid + YUBIKEY_UID_SIZE);
        decoded.aesKey.assign(key, key + YUBIKEY_KEY_SIZE);
        decoded.counter = yubikey_counter(token.ctr);
        decoded.sessionUse = token.use;
        result = std::move(decoded);
    }
    OPENSSL_cleanse(key, sizeof key);
    OPENSSL_cleanse(&token, sizeof token);
    return result;
}
