#ifndef ACCOUNT_ACTIVATION_H
#define ACCOUNT_ACTIVATION_H

#include <cstdint>
#include <functional>
#include <string>

#include "application/accounts/account_repository.h"
#include "application/auth/session_tokens.h"
#include "application/notifications/notifier.h"

// Initial allotment agreed in class (HU-08, RF-2.10).
constexpr int kInitialObjectsPerUser = 5;
// At most one new verification email per account in this time (RF-2.9),
// so the resend form cannot be used to flood someone's inbox.
constexpr std::int64_t kResendCooldownSeconds = 5 * 60;

struct ActivationOutcome {
    bool activated = false;
    std::string username;
    int objectsAssigned = 0;
};

// Email verification (HU-07) with the initial allotment (HU-08), and the
// resend of the verification link (RF-2.9).
class AccountActivation {
    public:
        using Clock = std::function<std::int64_t()>;

        AccountActivation(AccountRepository& accounts, SessionTokens& tokens, Notifier& notifier,
                          Clock now);

        // Any problem (unknown, used or expired token, account not pending)
        // gives the same "not activated" outcome.
        ActivationOutcome activate(const std::string& token, const std::string& clientIP);

        // Always "done" for the caller: whether the email belongs to a
        // pending account is never revealed.
        void resend(const std::string& email, const std::string& clientIP);

    private:
        AccountRepository& accounts;
        SessionTokens& tokens;
        Notifier& notifier;
        Clock now;
};

#endif // ACCOUNT_ACTIVATION_H
