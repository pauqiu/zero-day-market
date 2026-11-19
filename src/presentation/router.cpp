#include "presentation/router.h"

#include <vector>

#include "presentation/cgi/session_cookie.h"
#include "presentation/pages/auth_pages.h"
#include "presentation/pages/catalog_page.h"
#include "presentation/pages/history_page.h"
#include "presentation/pages/layout.h"
#include "presentation/pages/object_page.h"
#include "presentation/pages/register_page.h"

namespace {

// ------------------------------------------------------------ access levels

// What a route requires (control: row 5). Checked here, once, before any
// handler runs: a handler never has to remember to check it.
enum class Access {
    Public,               // anyone
    PendingSecondFactor,  // password verified, YubiKey still missing
    Authenticated,        // password AND YubiKey
    Admin                 // authenticated with the admin role
};

// Everything a handler needs about the current request.
struct Context {
    const CgiRequest& request;
    const UseCases& useCases;
    std::string token;                   // session cookie, if well formed
    std::optional<SessionInfo> session;  // its session, if still valid

    const std::string& script() const { return request.scriptName(); }

    // Only a full session shows the user in the header.
    std::optional<std::string> username() const
    {
        if (session && session->fullyAuthenticated) {
            return session->username;
        }
        return std::nullopt;
    }
};

using Handler = std::function<CgiResponse(const Context&)>;

struct Route {
    std::string path;
    Handler get;   // also serves HEAD
    Handler post;
    // Deny by default: a route that does not declare its access level
    // requires a full session.
    Access access = Access::Authenticated;
};

bool satisfies(Access access, const std::optional<SessionInfo>& session)
{
    switch (access) {
        case Access::Public:
            return true;
        case Access::PendingSecondFactor:
            return session && !session->fullyAuthenticated;
        case Access::Authenticated:
            return session && session->fullyAuthenticated;
        case Access::Admin:
            return session && session->fullyAuthenticated && session->isAdmin;
    }
    return false;
}

// ----------------------------------------------------------------- helpers

CgiResponse noStore(CgiResponse response)
{
    response.headers.emplace_back("Cache-Control", "no-store");
    return response;
}

// 303 See Other: after a POST, the browser loads the next page with GET, so
// reloading it never submits the form again.
CgiResponse redirect(const Context& c, const std::string& path)
{
    CgiResponse response;
    response.status = 303;
    response.headers.emplace_back("Location", c.script() + path);
    response.body = "";
    return noStore(response);
}

CgiResponse page(const Context& c, const std::string& title, const Html& body, int status = 200)
{
    CgiResponse response;
    response.status = status;
    response.body = renderPage(title, body, c.script(), c.username()).str();
    return response;
}

CgiResponse error(const Context& c, int status, const std::string& heading,
                  const std::string& message)
{
    return errorResponse(status, heading, message, c.script(), c.username());
}

// Common checks for every form submission.
std::optional<CgiResponse> rejectBadForm(const Context& c)
{
    if (c.request.bodyTooLarge()) {
        return error(c, 413, "Solicitud demasiado grande",
                     "Los datos enviados exceden el tamaño permitido.");
    }
    if (!c.request.hasFormBody()) {
        return error(c, 415, "Formato no admitido", "Los datos deben enviarse desde un formulario.");
    }
    return std::nullopt;
}

std::string formField(const Context& c, const char* name)
{
    return c.request.formParam(name).value_or("");
}

// ------------------------------------------------------- public pages

CgiResponse handleCatalog(const Context& c)
{
    int requestedPage = 1;
    if (const auto pageParam = c.request.queryParam("page")) {
        const auto parsed = parsePositiveInt(*pageParam);
        if (!parsed) {
            return error(c, 400, "Página inválida", "El número de página debe ser un entero positivo.");
        }
        requestedPage = *parsed;
    }
    const CatalogPage catalog = c.useCases.listCatalog(requestedPage);
    return page(c, "Catálogo", renderCatalogBody(catalog, c.script()));
}

// Reads ?id= for the object pages; on failure `failure` holds the 400.
std::optional<int> objectIDParam(const Context& c, CgiResponse& failure)
{
    const auto idParam = c.request.queryParam("id");
    if (!idParam) {
        failure = error(c, 400, "Solicitud incompleta", "Falta el identificador del objeto.");
        return std::nullopt;
    }
    const auto objectID = parsePositiveInt(*idParam);
    if (!objectID) {
        failure = error(c, 400, "Identificador inválido",
                        "El identificador del objeto debe ser un entero positivo.");
    }
    return objectID;
}

CgiResponse handleObject(const Context& c)
{
    CgiResponse failure;
    const std::optional<int> objectID = objectIDParam(c, failure);
    if (!objectID) {
        return failure;
    }
    const std::optional<ObjectDetail> detail = c.useCases.getObjectDetail(*objectID);
    if (!detail) {
        return error(c, 404, "Objeto no encontrado", "El objeto solicitado no existe en el mercado.");
    }
    return page(c, detail->object.name, renderObjectBody(*detail, c.script()));
}

CgiResponse handleHistory(const Context& c)
{
    CgiResponse failure;
    const std::optional<int> objectID = objectIDParam(c, failure);
    if (!objectID) {
        return failure;
    }
    const std::optional<ObjectHistory> history = c.useCases.getObjectHistory(*objectID);
    if (!history) {
        return error(c, 404, "Objeto no encontrado", "El objeto solicitado no existe en el mercado.");
    }
    return page(c, "Historial de " + history->objectName, renderHistoryBody(*history, c.script()));
}

// ----------------------------------------------------------- registration

CgiResponse handleRegisterForm(const Context& c)
{
    return noStore(page(c, "Crear cuenta", renderRegisterBody(c.script(), "", "", {})));
}

CgiResponse handleRegisterSubmit(const Context& c)
{
    if (auto bad = rejectBadForm(c)) {
        return *bad;
    }
    RegistrationRequest registration;
    registration.username = formField(c, "username");
    registration.email = formField(c, "email");
    registration.password = formField(c, "password");
    registration.passwordConfirmation = formField(c, "password_confirmation");
    registration.aesKey = formField(c, "aes_key");
    registration.otp = formField(c, "otp");
    registration.clientIP = c.request.remoteAddress();

    // Only these two fields may be written back into the form.
    const std::string username = registration.username;
    const std::string email = registration.email;

    const RegistrationResult result = c.useCases.registerUser(std::move(registration));
    if (result.success) {
        return noStore(page(c, "Revisa tu correo", renderRegisterDoneBody(c.script(), result)));
    }
    return noStore(page(c, "Crear cuenta",
                        renderRegisterBody(c.script(), username, email, result.errors), 422));
}

// ------------------------------------------------------- email verification

CgiResponse handleVerifyForm(const Context& c)
{
    const std::string token = c.request.queryParam("token").value_or("");
    if (!Authentication::isWellFormedToken(token)) {
        return noStore(page(c, "Enlace no válido", renderVerifyDoneBody(c.script(), ActivationOutcome{})));
    }
    return noStore(page(c, "Activa tu cuenta", renderVerifyBody(c.script(), token)));
}

CgiResponse handleVerifySubmit(const Context& c)
{
    if (auto bad = rejectBadForm(c)) {
        return *bad;
    }
    const ActivationOutcome outcome =
        c.useCases.activateAccount(formField(c, "token"), c.request.remoteAddress());
    return noStore(page(c, outcome.activated ? "Cuenta activada" : "Enlace no válido",
                        renderVerifyDoneBody(c.script(), outcome), outcome.activated ? 200 : 422));
}

CgiResponse handleResendForm(const Context& c)
{
    return noStore(page(c, "Reenviar el enlace de activación", renderResendBody(c.script())));
}

CgiResponse handleResendSubmit(const Context& c)
{
    if (auto bad = rejectBadForm(c)) {
        return *bad;
    }
    c.useCases.resendVerification(formField(c, "email"), c.request.remoteAddress());
    return noStore(page(c, "Revisa tu correo", renderResendDoneBody()));
}

// ------------------------------------------------------------------ login

// The same message for every failure of the first step (RF-2.17): it does
// not say whether the user exists, the password was wrong or the account
// is locked.
constexpr const char* kLoginFailed =
    "No fue posible iniciar sesión. Revisa tus datos; si fallaste varios intentos, "
    "espera 15 minutos antes de volver a intentarlo.";

std::optional<std::string> loginNotice(const Context& c)
{
    const std::string notice = c.request.queryParam("aviso").value_or("");
    // Only fixed texts: the parameter selects a message, it is never printed.
    if (notice == "expirada") return std::string("Tu sesión expiró. Vuelve a iniciar sesión.");
    if (notice == "bloqueo") return std::string(kLoginFailed);
    if (notice == "salida") return std::string("Cerraste sesión.");
    return std::nullopt;
}

CgiResponse handleLoginForm(const Context& c)
{
    if (c.session && c.session->fullyAuthenticated) {
        return redirect(c, "/cuenta");
    }
    return noStore(page(c, "Iniciar sesión",
                        renderLoginBody(c.script(), "", std::nullopt, loginNotice(c))));
}

CgiResponse handleLoginSubmit(const Context& c)
{
    if (auto bad = rejectBadForm(c)) {
        return *bad;
    }
    const std::string login = formField(c, "login");
    LoginOutcome outcome =
        c.useCases.loginWithPassword(login, formField(c, "password"), c.request.remoteAddress());
    if (!outcome.success) {
        return noStore(page(c, "Iniciar sesión",
                            renderLoginBody(c.script(), login, std::string(kLoginFailed), std::nullopt),
                            422));
    }
    CgiResponse response = redirect(c, "/login/yubikey");
    response.headers.emplace_back("Set-Cookie", sessionCookie(outcome.pendingToken));
    return response;
}

CgiResponse handleSecondFactorForm(const Context& c)
{
    return noStore(page(c, "Verifica tu YubiKey", renderSecondFactorBody(c.script(), std::nullopt)));
}

CgiResponse handleSecondFactorSubmit(const Context& c)
{
    if (auto bad = rejectBadForm(c)) {
        return *bad;
    }
    const SecondFactorOutcome outcome = c.useCases.verifySecondFactor(
        c.token, formField(c, "otp"), c.request.remoteAddress());

    switch (outcome.status) {
        case SecondFactorOutcome::Status::Success: {
            // A NEW identifier replaces the pending one (fixation, row 3).
            CgiResponse response = redirect(c, "/cuenta");
            response.headers.emplace_back("Set-Cookie", sessionCookie(outcome.sessionToken));
            return response;
        }
        case SecondFactorOutcome::Status::Rejected:
            return noStore(page(c, "Verifica tu YubiKey",
                                renderSecondFactorBody(c.script(),
                                    std::string("El código de la YubiKey no es válido. "
                                                "Toca tu YubiKey para generar uno nuevo.")),
                                422));
        case SecondFactorOutcome::Status::Locked: {
            CgiResponse response = redirect(c, "/login?aviso=bloqueo");
            response.headers.emplace_back("Set-Cookie", expiredSessionCookie());
            return response;
        }
        case SecondFactorOutcome::Status::SessionExpired:
            break;
    }
    CgiResponse response = redirect(c, "/login?aviso=expirada");
    response.headers.emplace_back("Set-Cookie", expiredSessionCookie());
    return response;
}

CgiResponse handleLogout(const Context& c)
{
    if (!c.token.empty() && c.useCases.logout) {
        c.useCases.logout(c.token, c.request.remoteAddress());
    }
    CgiResponse response = redirect(c, "/login?aviso=salida");
    response.headers.emplace_back("Set-Cookie", expiredSessionCookie());
    return response;
}

// ----------------------------------------------------------- user pages

CgiResponse handleAccount(const Context& c)
{
    return noStore(page(c, "Mi cuenta", renderAccountBody(c.session->username)));
}

// ------------------------------------------------------------ route table

const std::vector<Route>& routes()
{
    static const std::vector<Route> table = {
        {"", handleCatalog, nullptr, Access::Public},
        {"/", handleCatalog, nullptr, Access::Public},
        {"/catalogo", handleCatalog, nullptr, Access::Public},
        {"/objeto", handleObject, nullptr, Access::Public},
        {"/historial", handleHistory, nullptr, Access::Public},
        {"/registro", handleRegisterForm, handleRegisterSubmit, Access::Public},
        {"/verificar", handleVerifyForm, handleVerifySubmit, Access::Public},
        {"/verificar/reenviar", handleResendForm, handleResendSubmit, Access::Public},
        {"/login", handleLoginForm, handleLoginSubmit, Access::Public},
        {"/login/yubikey", handleSecondFactorForm, handleSecondFactorSubmit,
         Access::PendingSecondFactor},
        {"/logout", nullptr, handleLogout, Access::Public},
        {"/cuenta", handleAccount, nullptr},  // no level given: requires a full session
    };
    return table;
}

std::string allowedMethods(const Route& route)
{
    if (route.get && route.post) return "GET, HEAD, POST";
    if (route.post) return "POST";
    return "GET, HEAD";
}

}  // namespace

CgiResponse errorResponse(int status, const std::string& heading, const std::string& message,
                          const std::string& scriptName, const std::optional<std::string>& username)
{
    CgiResponse response;
    response.status = status;
    response.headers.emplace_back("Cache-Control", "no-store");
    response.body = renderPage(heading, renderErrorBody(heading, message, scriptName), scriptName,
                               username).str();
    return response;
}

CgiResponse route(const CgiRequest& request, const UseCases& useCases)
{
    if (request.queryTooLong()) {
        return errorResponse(414, "Dirección demasiado larga",
                             "La dirección solicitada excede el tamaño permitido.",
                             request.scriptName());
    }

    // The session, if the browser sent a well-formed cookie.
    Context c{request, useCases, "", std::nullopt};
    const std::optional<std::string> cookie = request.cookie(kSessionCookieName);
    if (cookie && Authentication::isWellFormedToken(*cookie)) {
        c.token = *cookie;
        if (useCases.resolveSession) {
            c.session = useCases.resolveSession(c.token);
        }
    }

    CgiResponse response;
    const Route* found = nullptr;
    for (const Route& candidate : routes()) {
        if (candidate.path == request.path()) {
            found = &candidate;
            break;
        }
    }

    const std::string& method = request.method();
    if (!found) {
        response = error(c, 404, "Página no encontrada", "La dirección solicitada no existe.");
    } else {
        const Handler& handler = (method == "GET" || method == "HEAD") ? found->get
                                 : method == "POST"                    ? found->post
                                                                       : Handler{};
        if (!handler) {
            response = error(c, 405, "Método no permitido",
                             "Esta dirección no admite ese tipo de solicitud.");
            response.headers.emplace_back("Allow", allowedMethods(*found));
        } else if (!satisfies(found->access, c.session)) {
            // Not allowed: to the login, except an authenticated user asking
            // for an admin page, who gets a plain refusal.
            if (found->access == Access::Admin && c.session && c.session->fullyAuthenticated) {
                response = error(c, 403, "Acceso denegado", "No tienes permiso para ver esta página.");
            } else {
                response = redirect(c, "/login");
            }
        } else {
            response = handler(c);
        }
    }

    // A cookie that no longer matches a valid session is deleted from the
    // browser, unless this response already sets a new one.
    if (cookie && !c.session) {
        bool setsCookie = false;
        for (const auto& [name, value] : response.headers) {
            setsCookie = setsCookie || name == "Set-Cookie";
        }
        if (!setsCookie) {
            response.headers.emplace_back("Set-Cookie", expiredSessionCookie());
        }
    }
    return response;
}
