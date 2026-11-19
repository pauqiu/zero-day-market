#include "presentation/pages/auth_pages.h"

Html renderLoginBody(const std::string& scriptName, const std::string& login,
                     const std::optional<std::string>& error,
                     const std::optional<std::string>& notice)
{
    TemplateContext context;
    context.setText("formAction", scriptName + "/login")
        .setText("registerUrl", scriptName + "/registro")
        .setText("login", login)
        .setFlag("hasError", error.has_value())
        .setText("error", error.value_or(""))
        .setFlag("hasNotice", notice.has_value())
        .setText("notice", notice.value_or(""));
    return renderNamedTemplate("login", context);
}

Html renderSecondFactorBody(const std::string& scriptName, const std::optional<std::string>& error)
{
    TemplateContext context;
    context.setText("formAction", scriptName + "/login/yubikey")
        .setFlag("hasError", error.has_value())
        .setText("error", error.value_or(""));
    return renderNamedTemplate("login_otp", context);
}

Html renderAccountBody(const std::string& username)
{
    TemplateContext context;
    context.setText("username", username);
    return renderNamedTemplate("account", context);
}
