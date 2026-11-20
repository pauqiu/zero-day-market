#include "infrastructure/mail/email_notifier.h"

#include <exception>
#include <iostream>

#include "application/common/local_time.h"
#include "infrastructure/mail/mail_config.h"
#include "infrastructure/mail/mail_message.h"
#include "infrastructure/mail/smtp_transport.h"

namespace {

// TODO: take it from the Local entity, like the page header.
constexpr const char* kStoreName = "Jester's Chest";

// One write per message: std::cerr is unbuffered, and Apache logs each write
// of a CGI as its own line, so "a" << "b" would be split into two lines.
void logLine(const std::string& text)
{
    std::cerr << ("app.cgi: " + text + "\n");
}

}  // namespace

EmailNotifier::EmailNotifier(std::string configPath, Clock now)
    : configPath(std::move(configPath)), now(std::move(now))
{
}

bool EmailNotifier::deliver(const std::string& to, const std::string& subject,
                            const std::function<std::string(const std::string&)>& body)
{
    try {
        const MailConfig config = loadMailConfig(configPath);
        const auto message = buildMailMessage(config.from, to, subject, body(config.appUrl), now());
        if (!message) {
            logLine("mail not sent: invalid address");
            return false;
        }
        SmtpTransport(config).send(to, *message);
        return true;
    } catch (const std::exception& error) {
        logLine(std::string("mail not sent: ") + error.what());
        return false;
    }
}

bool EmailNotifier::sendVerification(const std::string& to, const std::string& username,
                                     const std::string& token)
{
    return deliver(to, std::string("Activa tu cuenta en ") + kStoreName,
                   [&](const std::string& appUrl) {
        return "Hola, " + username + ":\n\n"
               "Recibimos una solicitud para crear una cuenta en " + kStoreName +
               " (Mercado el Día Cero) con este correo.\n\n"
               "Para activarla, abre este enlace en las próximas 24 horas:\n\n" +
               appUrl + "/verificar?token=" + token + "\n\n"
               "Si no fuiste tú, ignora este mensaje: sin activarla, la cuenta no se puede usar.\n";
    });
}

bool EmailNotifier::sendLoginNotice(const std::string& to, const std::string& username,
                                    std::int64_t when, const std::string& clientIP)
{
    return deliver(to, "Nuevo inicio de sesión en tu cuenta", [&](const std::string&) {
        return "Hola, " + username + ":\n\n"
               "Se inició sesión en tu cuenta de " + std::string(kStoreName) + " el " +
               formatLocalTime(when) + " (hora de Costa Rica), desde la dirección " + clientIP +
               ".\n\n"
               "Si fuiste tú, no tienes que hacer nada. Si no reconoces este acceso, "
               "comunícate con la administración del mercado.\n";
    });
}

bool EmailNotifier::sendLockNotice(const std::string& to, const std::string& username,
                                   std::int64_t when, const std::string& clientIP)
{
    return deliver(to, "Tu cuenta fue bloqueada temporalmente", [&](const std::string&) {
        return "Hola, " + username + ":\n\n"
               "Tu cuenta de " + std::string(kStoreName) + " se bloqueó durante 15 minutos el " +
               formatLocalTime(when) + " (hora de Costa Rica), tras varios intentos fallidos "
               "de inicio de sesión desde la dirección " + clientIP + ".\n\n"
               "Si fuiste tú, espera 15 minutos y vuelve a intentarlo. Si no reconoces estos "
               "intentos, comunícate con la administración del mercado.\n";
    });
}
