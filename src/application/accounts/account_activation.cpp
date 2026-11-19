#include "application/accounts/account_activation.h"

#include "application/accounts/register_user.h"  // kVerificationSeconds
#include "application/auth/authentication.h"     // token format
#include "application/common/wipe.h"
#include "domain/users/account_rules.h"

AccountActivation::AccountActivation(AccountRepository& accounts, SessionTokens& tokens,
                                     Notifier& notifier, Clock now)
    : accounts(accounts), tokens(tokens), notifier(notifier), now(std::move(now))
{
}

ActivationOutcome AccountActivation::activate(const std::string& token, const std::string& clientIP)
{
    ActivationOutcome outcome;
    if (!Authentication::isWellFormedToken(token)) {
        return outcome;
    }
    const ActivationResult result =
        accounts.activateAccount(tokens.hash(token), now(), kInitialObjectsPerUser, clientIP);
    if (!result.activated) {
        accounts.recordEvent("cuenta", "Enlace de verificacion invalido, usado o vencido",
                             clientIP, std::nullopt, "");
        return outcome;
    }
    outcome.activated = true;
    outcome.username = result.username;
    outcome.objectsAssigned = result.objectsAssigned;
    return outcome;
}

void AccountActivation::resend(const std::string& email, const std::string& clientIP)
{
    const auto normalized = normalizeEmail(email);
    if (!normalized) {
        return;
    }
    const auto pending = accounts.findPendingAccount(*normalized);
    if (!pending) {
        return;  // unknown or already active: nothing is sent, nothing is said
    }

    const std::int64_t current = now();
    // A token is created with 24 h of life: if the newest one is younger
    // than the cooldown, do not send another.
    if (pending->latestTokenExpiresAt &&
        *pending->latestTokenExpiresAt > current + kVerificationSeconds - kResendCooldownSeconds) {
        accounts.recordEvent("correo", "Reenvio de verificacion omitido (demasiado pronto)",
                             clientIP, pending->userID, "");
        return;
    }

    std::string token = tokens.newToken();
    accounts.replaceVerificationToken(pending->userID, tokens.hash(token),
                                      current + kVerificationSeconds, current);
    const bool sent = notifier.sendVerification(pending->email, pending->username, token);
    wipe(token);
    accounts.recordEvent("correo",
                         sent ? "Reenvio del correo de verificacion"
                              : "No se pudo reenviar el correo de verificacion",
                         clientIP, pending->userID, "");
}
