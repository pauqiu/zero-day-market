#ifndef MAIL_CONFIG_H
#define MAIL_CONFIG_H

#include <string>

// /etc/jesterschest/mail.conf (root:apache 0640), see deploy/mail.conf.example.
struct MailConfig {
    std::string url;              // smtp://172.24.131.105:587 (STARTTLS is then required)
    std::string username;         // the course account
    std::string password;
    std::string from;             // must be the authenticated account (Zimbra rejects others)
    std::string pinnedPublicKey;  // sha256//<base64>: the only server key accepted
    std::string caFile;           // alternative to the pin: the server's certificate
    std::string appUrl;           // https://www.jesterschest.internal/app.cgi, for links
};

// Throws MailError if the file is missing or incomplete. A configuration
// without a pinned key or a CA file is refused: mail is never sent to an
// unverified server.
MailConfig loadMailConfig(const std::string& path);

#endif // MAIL_CONFIG_H
