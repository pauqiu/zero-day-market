#include "presentation/router.h"

#include "presentation/pages/catalog_page.h"
#include "presentation/pages/history_page.h"
#include "presentation/pages/layout.h"
#include "presentation/pages/object_page.h"
#include "presentation/pages/register_page.h"

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

// Reads ?id= for the object pages. On failure, `error` holds the 400 response.
std::optional<int> objectIDParam(const CgiRequest& request, CgiResponse& error)
{
    const auto idParam = request.queryParam("id");
    if (!idParam) {
        error = errorResponse(400, "Solicitud incompleta",
                              "Falta el identificador del objeto.", request.scriptName());
        return std::nullopt;
    }
    const auto objectID = parsePositiveInt(*idParam);
    if (!objectID) {
        error = errorResponse(400, "Identificador inválido",
                              "El identificador del objeto debe ser un entero positivo.",
                              request.scriptName());
    }
    return objectID;
}

CgiResponse objectNotFound(const CgiRequest& request)
{
    return errorResponse(404, "Objeto no encontrado",
                         "El objeto solicitado no existe en el mercado.", request.scriptName());
}

CgiResponse handleObject(const CgiRequest& request, const UseCases& useCases)
{
    CgiResponse error;
    const std::optional<int> objectID = objectIDParam(request, error);
    if (!objectID) {
        return error;
    }

    const std::optional<ObjectDetail> detail = useCases.getObjectDetail(*objectID);
    if (!detail) {
        return objectNotFound(request);
    }

    CgiResponse response;
    response.body = renderPage(detail->object.name,
                               renderObjectBody(*detail, request.scriptName()),
                               request.scriptName()).str();
    return response;
}

CgiResponse handleHistory(const CgiRequest& request, const UseCases& useCases)
{
    CgiResponse error;
    const std::optional<int> objectID = objectIDParam(request, error);
    if (!objectID) {
        return error;
    }

    const std::optional<ObjectHistory> history = useCases.getObjectHistory(*objectID);
    if (!history) {
        return objectNotFound(request);
    }

    CgiResponse response;
    response.body = renderPage("Historial de " + history->objectName,
                               renderHistoryBody(*history, request.scriptName()),
                               request.scriptName()).str();
    return response;
}

bool isReadMethod(const std::string& method)
{
    return method == "GET" || method == "HEAD";
}

CgiResponse methodNotAllowed(const CgiRequest& request, const char* allowed)
{
    CgiResponse response = errorResponse(405, "Método no permitido",
                                          "Esta dirección no admite ese tipo de solicitud.",
                                          request.scriptName());
    response.headers.emplace_back("Allow", allowed);
    return response;
}

// Pages that carry or ask for secrets are never stored by the browser.
CgiResponse noStore(CgiResponse response)
{
    response.headers.emplace_back("Cache-Control", "no-store");
    return response;
}

CgiResponse handleRegisterForm(const CgiRequest& request)
{
    CgiResponse response;
    response.body = renderPage("Crear cuenta", renderRegisterBody(request.scriptName(), "", "", {}),
                               request.scriptName()).str();
    return noStore(response);
}

CgiResponse handleRegisterSubmit(const CgiRequest& request, const UseCases& useCases)
{
    if (request.bodyTooLarge()) {
        return errorResponse(413, "Solicitud demasiado grande",
                             "Los datos enviados exceden el tamaño permitido.", request.scriptName());
    }
    if (!request.hasFormBody()) {
        return errorResponse(415, "Formato no admitido",
                             "Los datos deben enviarse desde el formulario de registro.",
                             request.scriptName());
    }

    RegistrationRequest registration;
    registration.username = request.formParam("username").value_or("");
    registration.email = request.formParam("email").value_or("");
    registration.password = request.formParam("password").value_or("");
    registration.passwordConfirmation = request.formParam("password_confirmation").value_or("");
    registration.aesKey = request.formParam("aes_key").value_or("");
    registration.otp = request.formParam("otp").value_or("");
    registration.clientIP = request.remoteAddress();

    // Only these two fields may be written back into the form.
    const std::string username = registration.username;
    const std::string email = registration.email;

    const RegistrationResult result = useCases.registerUser(std::move(registration));

    CgiResponse response;
    if (result.success) {
        response.body = renderPage("Cuenta creada",
                                   renderRegisterDoneBody(request.scriptName(), result.username),
                                   request.scriptName()).str();
    } else {
        response.status = 422;
        response.body = renderPage("Crear cuenta",
                                   renderRegisterBody(request.scriptName(), username, email,
                                                      result.errors),
                                   request.scriptName()).str();
    }
    return noStore(response);
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
    if (request.queryTooLong()) {
        return errorResponse(414, "Dirección demasiado larga",
                             "La dirección solicitada excede el tamaño permitido.",
                             request.scriptName());
    }

    const std::string& path = request.path();
    const std::string& method = request.method();

    // Public pages: read-only.
    if (path.empty() || path == "/" || path == "/catalogo") {
        return isReadMethod(method) ? handleCatalog(request, useCases)
                                    : methodNotAllowed(request, "GET, HEAD");
    }
    if (path == "/objeto") {
        return isReadMethod(method) ? handleObject(request, useCases)
                                    : methodNotAllowed(request, "GET, HEAD");
    }
    if (path == "/historial") {
        return isReadMethod(method) ? handleHistory(request, useCases)
                                    : methodNotAllowed(request, "GET, HEAD");
    }

    // Registration: the form is read with GET; it only changes data with POST (control: row 4).
    if (path == "/registro") {
        if (isReadMethod(method)) {
            return handleRegisterForm(request);
        }
        if (method == "POST") {
            return handleRegisterSubmit(request, useCases);
        }
        return methodNotAllowed(request, "GET, HEAD, POST");
    }

    return errorResponse(404, "Página no encontrada",
                         "La dirección solicitada no existe.", request.scriptName());
}
