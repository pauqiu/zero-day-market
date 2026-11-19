// CGI entry point and composition root: the only place that knows every
// layer, and the one that connects the use cases to the infrastructure.

#include <exception>
#include <iostream>

#include "application/accounts/register_user.h"
#include "application/catalog/list_catalog.h"
#include "application/objects/get_object_detail.h"
#include "application/objects/get_object_history.h"
#include "infrastructure/crypto/aes_gcm_secret_box.h"
#include "infrastructure/crypto/scrypt_password_hasher.h"
#include "infrastructure/db/db_config.h"
#include "infrastructure/db/mariadb_account_repository.h"
#include "infrastructure/db/mariadb_catalog_repository.h"
#include "infrastructure/db/mariadb_connection.h"
#include "infrastructure/db/mariadb_object_detail_repository.h"
#include "infrastructure/db/mariadb_object_history_repository.h"
#include "presentation/cgi/cgi_request.h"
#include "infrastructure/yubikey/libyubikey_otp_decoder.h"
#include "presentation/router.h"

#ifndef JC_DB_CONFIG_PATH
#error "JC_DB_CONFIG_PATH must be defined by the build"
#endif
#ifndef JC_MASTER_KEY_PATH
#error "JC_MASTER_KEY_PATH must be defined by the build"
#endif

int main()
{
    const CgiRequest request = CgiRequest::fromEnvironment();

    UseCases useCases;
    useCases.listCatalog = [](int requestedPage) {
        const DbConfig config = loadDbConfig(JC_DB_CONFIG_PATH);
        MariaDbConnection connection(config);
        MariaDbCatalogRepository repository(connection);
        ListCatalog listCatalog(repository);
        return listCatalog.execute(requestedPage);
    };
    useCases.getObjectDetail = [](int objectID) {
        const DbConfig config = loadDbConfig(JC_DB_CONFIG_PATH);
        MariaDbConnection connection(config);
        MariaDbObjectDetailRepository repository(connection);
        GetObjectDetail getObjectDetail(repository);
        return getObjectDetail.execute(objectID);
    };
    useCases.getObjectHistory = [](int objectID) {
        const DbConfig config = loadDbConfig(JC_DB_CONFIG_PATH);
        MariaDbConnection connection(config);
        MariaDbObjectHistoryRepository repository(connection);
        GetObjectHistory getObjectHistory(repository);
        return getObjectHistory.execute(objectID);
    };
    useCases.registerUser = [](RegistrationRequest request) {
        const DbConfig config = loadDbConfig(JC_DB_CONFIG_PATH);
        MariaDbConnection connection(config);
        MariaDbAccountRepository accounts(connection);
        ScryptPasswordHasher hasher;
        AesGcmSecretBox secrets = AesGcmSecretBox::fromKeyFile(JC_MASTER_KEY_PATH);
        LibyubikeyOtpDecoder decoder;
        RegisterUser registerUser(accounts, hasher, secrets, decoder);
        return registerUser.execute(std::move(request));
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
