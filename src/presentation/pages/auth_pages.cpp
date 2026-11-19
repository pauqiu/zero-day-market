#include "presentation/pages/auth_pages.h"

Html renderLoginBody(const std::string& scriptName, const std::string& login,
                     const std::optional<std::string>& error,
                     const std::optional<std::string>& notice)
{
    TemplateContext context;
    context.setText("formAction", scriptName + "/login")
        .setText("registerUrl", scriptName + "/registro")
        .setText("resendUrl", scriptName + "/verificar/reenviar")
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

Html renderVerifyBody(const std::string& scriptName, const std::string& token)
{
    TemplateContext context;
    context.setText("formAction", scriptName + "/verificar").setText("token", token);
    return renderNamedTemplate("verify", context);
}

Html renderVerifyDoneBody(const std::string& scriptName, const ActivationOutcome& outcome)
{
    TemplateContext context;
    context.setFlag("activated", outcome.activated)
        .setText("username", outcome.username)
        .setFlag("hasObjects", outcome.objectsAssigned > 0)
        .setNumber("objects", outcome.objectsAssigned)
        .setFlag("oneObject", outcome.objectsAssigned == 1)
        .setText("loginUrl", scriptName + "/login")
        .setText("resendUrl", scriptName + "/verificar/reenviar");
    return renderNamedTemplate("verify_done", context);
}

Html renderResendBody(const std::string& scriptName)
{
    TemplateContext context;
    context.setText("formAction", scriptName + "/verificar/reenviar");
    return renderNamedTemplate("resend", context);
}

Html renderResendDoneBody()
{
    return renderNamedTemplate("resend_done", TemplateContext{});
}
