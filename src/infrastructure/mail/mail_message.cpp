#include "infrastructure/mail/mail_message.h"

#include <openssl/evp.h>

#include <ctime>
#include <vector>

#include "domain/users/account_rules.h"

namespace {

std::string base64(const std::string& data)
{
    std::vector<unsigned char> out(4 * ((data.size() + 2) / 3) + 1);
    const int length = EVP_EncodeBlock(out.data(), reinterpret_cast<const unsigned char*>(data.data()),
                                       static_cast<int>(data.size()));
    return std::string(reinterpret_cast<const char*>(out.data()), static_cast<std::size_t>(length));
}

// Body lines of at most 76 characters, as MIME requires.
std::string wrap76(const std::string& encoded)
{
    std::string out;
    for (std::size_t i = 0; i < encoded.size(); i += 76) {
        out += encoded.substr(i, 76) + "\r\n";
    }
    return out;
}

std::string rfc5322Date(std::int64_t epoch)
{
    const std::time_t time = static_cast<std::time_t>(epoch);
    std::tm parts{};
    gmtime_r(&time, &parts);
    char buffer[64];
    std::strftime(buffer, sizeof buffer, "%a, %d %b %Y %H:%M:%S +0000", &parts);
    return buffer;
}

}  // namespace

bool isPlainAddress(const std::string& address)
{
    // normalizeEmail only accepts letters, digits and . _ % + - around one @.
    const auto normalized = normalizeEmail(address);
    return normalized.has_value();
}

std::optional<std::string> buildMailMessage(const std::string& from, const std::string& to,
                                            const std::string& subject, const std::string& body,
                                            std::int64_t dateEpoch)
{
    if (!isPlainAddress(from) || !isPlainAddress(to)) {
        return std::nullopt;
    }
    std::string message;
    message += "Date: " + rfc5322Date(dateEpoch) + "\r\n";
    message += "From: " + from + "\r\n";
    message += "To: " + to + "\r\n";
    message += "Subject: =?UTF-8?B?" + base64(subject) + "?=\r\n";  // RFC 2047
    message += "MIME-Version: 1.0\r\n";
    message += "Content-Type: text/plain; charset=UTF-8\r\n";
    message += "Content-Transfer-Encoding: base64\r\n";
    message += "\r\n";
    message += wrap76(base64(body));
    return message;
}
