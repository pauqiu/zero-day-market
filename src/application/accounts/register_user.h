#ifndef REGISTER_USER_H
#define REGISTER_USER_H

#include <string>
#include <vector>

#include "application/accounts/account_repository.h"
#include "application/accounts/password_hasher.h"
#include "application/accounts/secret_box.h"
#include "application/accounts/yubikey_otp_decoder.h"

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
    std::vector<RegistrationError> errors;
};

// Use case HU-06 with the YubiKey registration of HU-10.
class RegisterUser {
    public:
        RegisterUser(AccountRepository& accounts, PasswordHasher& hasher,
                     SecretBox& secrets, YubikeyOtpDecoder& decoder);

        // Takes the request by value: its secrets are wiped before returning.
        RegistrationResult execute(RegistrationRequest request);

    private:
        AccountRepository& accounts;
        PasswordHasher& hasher;
        SecretBox& secrets;
        YubikeyOtpDecoder& decoder;
};

#endif // REGISTER_USER_H
