#ifndef AUTH_PAGES_H
#define AUTH_PAGES_H

#include <optional>
#include <string>

#include "application/accounts/account_activation.h"
#include "presentation/templates/template_engine.h"

Html renderLoginBody(const std::string& scriptName, const std::string& login,
                     const std::optional<std::string>& error,
                     const std::optional<std::string>& notice);

Html renderSecondFactorBody(const std::string& scriptName, const std::optional<std::string>& error);

Html renderAccountBody(const std::string& username);

// Email verification (HU-07) and resend (RF-2.9).
Html renderVerifyBody(const std::string& scriptName, const std::string& token);
Html renderVerifyDoneBody(const std::string& scriptName, const ActivationOutcome& outcome);
Html renderResendBody(const std::string& scriptName);
Html renderResendDoneBody();

#endif // AUTH_PAGES_H
