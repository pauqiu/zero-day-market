#include "infrastructure/mail/mail_config.h"

#include <fstream>

#include "infrastructure/mail/mail_error.h"

namespace {

std::string trim(const std::string& text)
{
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return "";
    }
    return text.substr(first, text.find_last_not_of(" \t\r\n") - first + 1);
}

}  // namespace

MailConfig loadMailConfig(const std::string& path)
{
    std::ifstream file(path);
    if (!file) {
        throw MailError("cannot open mail configuration: " + path);
    }
    MailConfig config;
    std::string line;
    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') {
            continue;
        }
        const auto equals = line.find('=');
        if (equals == std::string::npos) {
            throw MailError("malformed line in mail configuration");
        }
        const std::string key = trim(line.substr(0, equals));
        const std::string value = trim(line.substr(equals + 1));
        if (key == "url") config.url = value;
        else if (key == "username") config.username = value;
        else if (key == "password") config.password = value;
        else if (key == "from") config.from = value;
        else if (key == "pinned_public_key") config.pinnedPublicKey = value;
        else if (key == "ca_file") config.caFile = value;
        else if (key == "app_url") config.appUrl = value;
    }
    if (config.url.rfind("smtp://", 0) != 0 && config.url.rfind("smtps://", 0) != 0) {
        throw MailError("mail url must start with smtp:// or smtps://");
    }
    // Links in emails must use HTTPS; http://localhost is accepted only for
    // the local development environment.
    const bool httpsUrl = config.appUrl.rfind("https://", 0) == 0;
    const bool localUrl = config.appUrl.rfind("http://localhost/", 0) == 0;
    if (config.from.empty() || (!httpsUrl && !localUrl)) {
        throw MailError("mail configuration needs from and an https:// app_url");
    }
    if (config.pinnedPublicKey.empty() && config.caFile.empty()) {
        throw MailError("mail configuration needs pinned_public_key or ca_file");
    }
    if (!config.pinnedPublicKey.empty() && config.pinnedPublicKey.rfind("sha256//", 0) != 0) {
        throw MailError("pinned_public_key must look like sha256//<base64>");
    }
    return config;
}
