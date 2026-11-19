#include "presentation/pages/register_page.h"

std::string registrationErrorMessage(RegistrationError error)
{
    switch (error) {
        case RegistrationError::InvalidUsername:
            return "El nombre de usuario debe tener de 3 a 32 caracteres: letras, números, guion o guion bajo.";
        case RegistrationError::InvalidEmail:
            return "El correo electrónico no es válido.";
        case RegistrationError::PasswordTooShort:
            return "La contraseña debe tener al menos 12 caracteres.";
        case RegistrationError::PasswordTooLong:
            return "La contraseña no puede tener más de 128 caracteres.";
        case RegistrationError::PasswordTooSimple:
            return "La contraseña debe combinar al menos 3 tipos de caracteres: minúsculas, mayúsculas, números y otros símbolos.";
        case RegistrationError::PasswordMismatch:
            return "Las contraseñas no coinciden.";
        case RegistrationError::InvalidAesKeyFormat:
            return "La llave AES debe tener exactamente 32 caracteres hexadecimales.";
        case RegistrationError::InvalidOtpFormat:
            return "El código de la YubiKey no tiene el formato esperado. Toca tu YubiKey en el campo correspondiente.";
        case RegistrationError::YubikeyRejected:
            return "Esta YubiKey no se puede registrar. Verifica la llave AES y genera un código nuevo.";
        case RegistrationError::UsernameTaken:
            return "Ese nombre de usuario ya está en uso.";
        case RegistrationError::EmailTaken:
            return "Ese correo electrónico ya está registrado.";
    }
    return "No se pudo completar el registro.";
}

Html renderRegisterBody(const std::string& scriptName, const std::string& username,
                        const std::string& email, const std::vector<RegistrationError>& errors)
{
    std::vector<TemplateContext> messages;
    for (const RegistrationError error : errors) {
        TemplateContext item;
        item.setText("message", registrationErrorMessage(error));
        messages.push_back(std::move(item));
    }
    TemplateContext context;
    context.setText("formAction", scriptName + "/registro")
        .setText("username", username)
        .setText("email", email)
        .setFlag("hasErrors", !messages.empty())
        .setList("errors", std::move(messages));
    return renderNamedTemplate("register", context);
}

Html renderRegisterDoneBody(const std::string& scriptName, const RegistrationResult& result)
{
    TemplateContext context;
    context.setText("username", result.username)
        .setText("email", result.email)
        .setFlag("sent", result.verificationSent)
        .setText("resendUrl", scriptName + "/verificar/reenviar");
    return renderNamedTemplate("register_done", context);
}
