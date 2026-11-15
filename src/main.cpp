// CGI entry point and composition root: the only place that knows every
// layer, and the one that connects the use cases to the infrastructure.

#include <exception>
#include <iostream>

#include "application/catalog/list_catalog.h"
#include "infrastructure/db/db_config.h"
#include "infrastructure/db/mariadb_catalog_repository.h"
#include "infrastructure/db/mariadb_connection.h"
#include "presentation/cgi/cgi_request.h"
#include "presentation/router.h"

#ifndef JC_DB_CONFIG_PATH
#error "JC_DB_CONFIG_PATH must be defined by the build"
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
