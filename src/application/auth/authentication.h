#ifndef AUTHENTICATION_H
#define AUTHENTICATION_H

#include <cstdint>
#include <functional>
#include <optional>
#include <string>

#include "application/accounts/password_hasher.h"
#include "application/accounts/secret_box.h"
#include "application/accounts/yubikey_otp_decoder.h"
#include "application/auth/auth_repository.h"
#include "application/auth/session_tokens.h"

// Policy of the login flow (security architecture, rows 3, 5 and 20).
struct AuthPolicy {
    std::int64_t pendingSessionSeconds = 5 * 60;   // between password and YubiKey
    std::int64_t idleSessionSeconds = 15 * 60;     // inactivity
    std::int64_t maxSessionSeconds = 8 * 60 * 60;  // absolute
    int maxFailedAttempts = 3;                     // password or OTP failures
    std::int64_t lockSeconds = 15 * 60;            // temporary lockout
};

struct LoginOutcome {
    bool success = false;
    std::string pendingToken;  // session that only allows the YubiKey step
};

struct SecondFactorOutcome {
    enum class Status { Success, Rejected, Locked, SessionExpired };
    Status status = Status::Rejected;
    std::string sessionToken;  // new session (the pending one is deleted)
};

struct SessionInfo {
    int userID = 0;
    std::string username;
    bool isAdmin = false;
    bool fullyAuthenticated = false;  // password AND YubiKey
};

// The login flow: password (HU-09), YubiKey (HU-11), session checks and
// logout (HU-12). One class, because the four operations share the same
// policy and dependencies.
class Authentication {
    public:
        using Clock = std::function<std::int64_t()>;

        Authentication(AuthRepository& repository, PasswordHasher& hasher, SecretBox& secrets,
                       YubikeyOtpDecoder& decoder, SessionTokens& tokens, Clock now,
                       AuthPolicy policy = AuthPolicy{});

        // `login` is the username or the email. The same failure for every
        // reason: unknown user, wrong password, locked or inactive account.
        LoginOutcome loginWithPassword(std::string login, std::string password,
                                       const std::string& clientIP);

        SecondFactorOutcome verifySecondFactor(const std::string& pendingToken, std::string otp,
                                               const std::string& clientIP);

        // The session of a request, if it is still valid. Expired or idle
        // sessions are deleted. Refreshes the inactivity timer.
        std::optional<SessionInfo> resolveSession(const std::string& token);

        void logout(const std::string& token, const std::string& clientIP);

        // 64 lowercase hex characters: anything else is not even looked up.
        static bool isWellFormedToken(const std::string& token);

    private:
        SecondFactorOutcome rejectSecondFactor(const SessionRecord& pending, const std::string& pendingHash,
                                               const std::string& clientIP, const char* reason);

        AuthRepository& repository;
        PasswordHasher& hasher;
        SecretBox& secrets;
        YubikeyOtpDecoder& decoder;
        SessionTokens& tokens;
        Clock now;
        AuthPolicy policy;
};

#endif // AUTHENTICATION_H
