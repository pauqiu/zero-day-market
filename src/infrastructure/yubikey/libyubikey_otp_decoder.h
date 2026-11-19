#ifndef LIBYUBIKEY_OTP_DECODER_H
#define LIBYUBIKEY_OTP_DECODER_H

#include "application/accounts/yubikey_otp_decoder.h"

// Yubico OTP validated locally with libyubikey (Yubico's C library).
class LibyubikeyOtpDecoder : public YubikeyOtpDecoder {
    public:
        std::optional<DecodedOtp> decode(std::string_view otp, std::string_view aesKeyHex) override;
};

#endif // LIBYUBIKEY_OTP_DECODER_H
