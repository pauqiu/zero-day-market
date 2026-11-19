#ifndef REGISTER_USER_H
#define REGISTER_USER_H

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "application/accounts/account_repository.h"
#include "application/accounts/password_hasher.h"
#include "application/accounts/secret_box.h"
#include "application/accounts/yubikey_otp_decoder.h"
#include "application/auth/session_tokens.h"
#include "application/notifications/notifier.h"

// Lifetime of the email verification link.
constexpr std::int64_t kVerificationSeconds = 24 * 60 * 60;

struct RegistrationRequest {
    std::string username;
    std::string email;
    std::string password;
    std::string passwordConfirmation;
    std::string aesKey;  // 32 hex characters, typed by the user
    std::string otp;     // 44 modhex characters, typed by the YubiKey
    std::string clientIP;
};

enum class RegistrationError {
    InvalidUsername,
    InvalidEmail,
    PasswordTooShort,
    PasswordTooLong,
    PasswordTooSimple,
    PasswordMismatch,
    InvalidAesKeyFormat,
    InvalidOtpFormat,
    YubikeyRejected,  // wrong key, invalid OTP or key already registered: same message
    UsernameTaken,
    EmailTaken
};

struct RegistrationResult {
    bool success = false;
    int userID = 0;
    std::string username;  // normalized, for the confirmation page
    std::string email;     // where the verification link was sent
    bool verificationSent = false;
    std::vector<RegistrationError> errors;
};

// Use case HU-06 with the YubiKey registration of HU-10.
class RegisterUser {
    public:
        using Clock = std::function<std::int64_t()>;

        RegisterUser(AccountRepository& accounts, PasswordHasher& hasher, SecretBox& secrets,
                     YubikeyOtpDecoder& decoder, SessionTokens& tokens, Notifier& notifier,
                     Clock now);

        // Takes the request by value: its secrets are wiped before returning.
        RegistrationResult execute(RegistrationRequest request);

    private:
        AccountRepository& accounts;
        PasswordHasher& hasher;
        SecretBox& secrets;
        YubikeyOtpDecoder& decoder;
        SessionTokens& tokens;
        Notifier& notifier;
        Clock now;
};

#endif // REGISTER_USER_H
