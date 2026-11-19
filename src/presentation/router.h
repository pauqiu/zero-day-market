#ifndef ROUTER_H
#define ROUTER_H

#include <functional>
#include <optional>
#include <string>

#include "application/accounts/account_activation.h"
#include "application/accounts/register_user.h"
#include "application/auth/authentication.h"
#include "application/catalog/catalog_item.h"
#include "application/objects/object_detail.h"
#include "application/objects/object_history.h"
#include "presentation/cgi/cgi_request.h"
#include "presentation/cgi/cgi_response.h"

// Use cases the router can call. main.cpp wires them to the real
// infrastructure, so presentation never depends on MariaDB directly.
struct UseCases {
    std::function<CatalogPage(int requestedPage)> listCatalog;
    std::function<std::optional<ObjectDetail>(int objectID)> getObjectDetail;
    std::function<std::optional<ObjectHistory>(int objectID)> getObjectHistory;
    std::function<RegistrationResult(RegistrationRequest request)> registerUser;
    std::function<ActivationOutcome(const std::string& token, const std::string& clientIP)> activateAccount;
    std::function<void(const std::string& email, const std::string& clientIP)> resendVerification;

    std::function<LoginOutcome(std::string login, std::string password,
                               const std::string& clientIP)> loginWithPassword;
    std::function<SecondFactorOutcome(const std::string& pendingToken, std::string otp,
                                      const std::string& clientIP)> verifySecondFactor;
    std::function<std::optional<SessionInfo>(const std::string& token)> resolveSession;
    std::function<void(const std::string& token, const std::string& clientIP)> logout;
};

CgiResponse route(const CgiRequest& request, const UseCases& useCases);

CgiResponse errorResponse(int status, const std::string& heading, const std::string& message,
                          const std::string& scriptName,
                          const std::optional<std::string>& username = std::nullopt);

#endif // ROUTER_H
