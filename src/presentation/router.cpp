#include "presentation/router.h"

#include "presentation/pages/catalog_page.h"
#include "presentation/pages/layout.h"

namespace {

CgiResponse handleCatalog(const CgiRequest& request, const UseCases& useCases)
{
    int requestedPage = 1;
    if (const auto pageParam = request.queryParam("page")) {
        const auto parsed = parsePositiveInt(*pageParam);
        if (!parsed) {
            return errorResponse(400, "Página inválida",
                                 "El número de página debe ser un entero positivo.",
                                 request.scriptName());
        }
        requestedPage = *parsed;
    }

    const CatalogPage page = useCases.listCatalog(requestedPage);

    CgiResponse response;
    response.body = renderLayout("Catálogo", renderCatalogBody(page, request.scriptName()),
                                 request.scriptName());
    return response;
}

}  // namespace

CgiResponse errorResponse(int status, const std::string& heading,
                          const std::string& message, const std::string& scriptName)
{
    CgiResponse response;
    response.status = status;
    response.headers.emplace_back("Cache-Control", "no-store");
    response.body = renderLayout(heading, renderErrorBody(heading, message, scriptName),
                                 scriptName);
    return response;
}

CgiResponse route(const CgiRequest& request, const UseCases& useCases)
{
    const std::string& method = request.method();
    if (method != "GET" && method != "HEAD") {
        CgiResponse response = errorResponse(405, "Método no permitido",
                                             "Esta página solo admite consultas.",
                                             request.scriptName());
        response.headers.emplace_back("Allow", "GET, HEAD");
        return response;
    }

    if (request.queryTooLong()) {
        return errorResponse(414, "Dirección demasiado larga",
                             "La dirección solicitada excede el tamaño permitido.",
                             request.scriptName());
    }

    const std::string& path = request.path();
    if (path.empty() || path == "/" || path == "/catalogo") {
        return handleCatalog(request, useCases);
    }

    return errorResponse(404, "Página no encontrada",
                         "La dirección solicitada no existe.", request.scriptName());
}
