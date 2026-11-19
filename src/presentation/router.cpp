#include "presentation/router.h"

#include "presentation/pages/catalog_page.h"
#include "presentation/pages/layout.h"
#include "presentation/pages/object_page.h"

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
    response.body = renderPage("Catálogo", renderCatalogBody(page, request.scriptName()),
                               request.scriptName()).str();
    return response;
}

CgiResponse handleObject(const CgiRequest& request, const UseCases& useCases)
{
    const auto idParam = request.queryParam("id");
    if (!idParam) {
        return errorResponse(400, "Solicitud incompleta",
                             "Falta el identificador del objeto.", request.scriptName());
    }
    const auto objectID = parsePositiveInt(*idParam);
    if (!objectID) {
        return errorResponse(400, "Identificador inválido",
                             "El identificador del objeto debe ser un entero positivo.",
                             request.scriptName());
    }

    const std::optional<ObjectDetail> detail = useCases.getObjectDetail(*objectID);
    if (!detail) {
        return errorResponse(404, "Objeto no encontrado",
                             "El objeto solicitado no existe en el mercado.", request.scriptName());
    }

    CgiResponse response;
    response.body = renderPage(detail->object.name,
                               renderObjectBody(*detail, request.scriptName()),
                               request.scriptName()).str();
    return response;
}

}  // namespace

CgiResponse errorResponse(int status, const std::string& heading,
                          const std::string& message, const std::string& scriptName)
{
    CgiResponse response;
    response.status = status;
    response.headers.emplace_back("Cache-Control", "no-store");
    response.body = renderPage(heading, renderErrorBody(heading, message, scriptName),
                               scriptName).str();
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
    if (path == "/objeto") {
        return handleObject(request, useCases);
    }

    return errorResponse(404, "Página no encontrada",
                         "La dirección solicitada no existe.", request.scriptName());
}
