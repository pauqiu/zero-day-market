// CGI entry point and composition root: the only place that knows every
// layer, and the one that connects the use cases to the infrastructure.

#include <exception>
#include <iostream>
#include <memory>

#include "application/accounts/register_user.h"
#include "application/auth/authentication.h"
#include "application/catalog/list_catalog.h"
#include "application/objects/get_object_detail.h"
#include "application/objects/get_object_history.h"
#include "infrastructure/crypto/aes_gcm_secret_box.h"
#include "infrastructure/crypto/random_session_tokens.h"
#include "infrastructure/crypto/scrypt_password_hasher.h"
#include "infrastructure/db/db_config.h"
#include "infrastructure/db/mariadb_account_repository.h"
#include "infrastructure/db/mariadb_auth_repository.h"
#include "infrastructure/db/mariadb_catalog_repository.h"
#include "infrastructure/db/mariadb_connection.h"
#include "infrastructure/db/mariadb_object_detail_repository.h"
#include "infrastructure/db/mariadb_object_history_repository.h"
#include "infrastructure/yubikey/libyubikey_otp_decoder.h"
#include "presentation/cgi/cgi_request.h"
#include "presentation/router.h"

#ifndef JC_DB_CONFIG_PATH
#error "JC_DB_CONFIG_PATH must be defined by the build"
#endif
#ifndef JC_MASTER_KEY_PATH
#error "JC_MASTER_KEY_PATH must be defined by the build"
#endif

namespace {

// Everything is created on first use and lives for this request (this
// process). A request opens at most one database connection, and only if
// it needs one; the master key is read only by requests that use it.
class Services {
    public:
        MariaDbConnection& db()
        {
            if (!connection) {
                connection = std::make_unique<MariaDbConnection>(loadDbConfig(JC_DB_CONFIG_PATH));
            }
            return *connection;
        }

        AesGcmSecretBox& secrets()
        {
            if (!secretBox) {
                secretBox = std::make_unique<AesGcmSecretBox>(
                    AesGcmSecretBox::fromKeyFile(JC_MASTER_KEY_PATH));
            }
            return *secretBox;
        }

        Authentication& authentication()
        {
            if (!auth) {
                authRepository = std::make_unique<MariaDbAuthRepository>(db());
                auth = std::make_unique<Authentication>(*authRepository, hasher, secrets(), decoder,
                                                        tokens, GetObjectDetail::systemClock);
            }
            return *auth;
        }

        ScryptPasswordHasher hasher;
        LibyubikeyOtpDecoder decoder;
        RandomSessionTokens tokens;

    private:
        std::unique_ptr<MariaDbConnection> connection;
        std::unique_ptr<AesGcmSecretBox> secretBox;
        std::unique_ptr<MariaDbAuthRepository> authRepository;
        std::unique_ptr<Authentication> auth;
};

}  // namespace

int main()
{
    const CgiRequest request = CgiRequest::fromEnvironment();
    Services services;

    UseCases useCases;
    useCases.listCatalog = [&services](int requestedPage) {
        MariaDbCatalogRepository repository(services.db());
        return ListCatalog(repository).execute(requestedPage);
    };
    useCases.getObjectDetail = [&services](int objectID) {
        MariaDbObjectDetailRepository repository(services.db());
        return GetObjectDetail(repository).execute(objectID);
    };
    useCases.getObjectHistory = [&services](int objectID) {
        MariaDbObjectHistoryRepository repository(services.db());
        return GetObjectHistory(repository).execute(objectID);
    };
    useCases.registerUser = [&services](RegistrationRequest registration) {
        MariaDbAccountRepository accounts(services.db());
        RegisterUser registerUser(accounts, services.hasher, services.secrets(), services.decoder);
        return registerUser.execute(std::move(registration));
    };
    useCases.loginWithPassword = [&services](std::string login, std::string password,
                                             const std::string& clientIP) {
        return services.authentication().loginWithPassword(std::move(login), std::move(password),
                                                           clientIP);
    };
    useCases.verifySecondFactor = [&services](const std::string& pendingToken, std::string otp,
                                              const std::string& clientIP) {
        return services.authentication().verifySecondFactor(pendingToken, std::move(otp), clientIP);
    };
    useCases.resolveSession = [&services](const std::string& token) {
        return services.authentication().resolveSession(token);
    };
    useCases.logout = [&services](const std::string& token, const std::string& clientIP) {
        services.authentication().logout(token, clientIP);
    };

    CgiResponse response;
    try {
        response = route(request, useCases);
    } catch (const std::exception& error) {
        // Details go to Apache's error log, never to the visitor.
        std::cerr << "app.cgi: " << error.what() << '\n';
        response = errorResponse(500, "Error del servidor",
                                 "No se pudo completar la solicitud. Intenta de nuevo más tarde.",
                                 request.scriptName());
    }

    writeResponse(response, std::cout, request.method() != "HEAD");
    return 0;
}
