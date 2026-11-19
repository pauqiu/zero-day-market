#ifndef AUTH_PAGES_H
#define AUTH_PAGES_H

#include <optional>
#include <string>

#include "presentation/templates/template_engine.h"

Html renderLoginBody(const std::string& scriptName, const std::string& login,
                     const std::optional<std::string>& error,
                     const std::optional<std::string>& notice);

Html renderSecondFactorBody(const std::string& scriptName, const std::optional<std::string>& error);

Html renderAccountBody(const std::string& username);

#endif // AUTH_PAGES_H
