#ifndef YUBIKEY_OTP_DECODER_H
#define YUBIKEY_OTP_DECODER_H

#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Contents of a Yubico OTP whose encrypted block was decrypted with the
// right AES key (its CRC is correct).
struct DecodedOtp {
    std::string publicID;                  // first 12 modhex characters of the OTP
    std::vector<unsigned char> privateID;  // 6 bytes inside the encrypted block
    std::vector<unsigned char> aesKey;     // the 16-byte AES key that decrypted it
    int counter = 0;                       // usage counter (15 bits)
    int sessionUse = 0;                    // session counter (8 bits)
};

// Port: decodes and decrypts a Yubico OTP with an AES key given in hex.
class YubikeyOtpDecoder {
    public:
        virtual ~YubikeyOtpDecoder() = default;

        // Empty if the formats are wrong or the AES key does not decrypt the
        // OTP into a block with a valid CRC.
        virtual std::optional<DecodedOtp> decode(std::string_view otp,
                                                 std::string_view aesKeyHex) = 0;
};

#endif // YUBIKEY_OTP_DECODER_H
