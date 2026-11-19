#ifndef SMTP_TRANSPORT_H
#define SMTP_TRANSPORT_H

#include <string>

#include "infrastructure/mail/mail_config.h"

// Sends one already built message through the course mail server with
// libcurl. Throws MailError on any failure.
class SmtpTransport {
    public:
        explicit SmtpTransport(MailConfig config);

        void send(const std::string& to, const std::string& message);

    private:
        MailConfig config;
};

#endif // SMTP_TRANSPORT_H
