#include "presentation/cgi/cgi_request.h"

#include <cstdio>
#include <cstdlib>

namespace {

std::string environment(const char* name)
{
    const char* value = std::getenv(name);
    return value != nullptr ? value : "";
}

int hexValue(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

}  // namespace

CgiRequest CgiRequest::fromEnvironment()
{
    const std::string method = environment("REQUEST_METHOD");
    std::string body;
    bool tooLarge = false;
    if (method == "POST") {
        // CONTENT_LENGTH comes from the client: validated before reading.
        const std::string declared = environment("CONTENT_LENGTH");
        std::size_t length = 0;
        bool valid = !declared.empty() && declared.size() <= 9;
        for (const char c : declared) {
            valid = valid && c >= '0' && c <= '9';
        }
        if (valid) {
            length = static_cast<std::size_t>(std::stoul(declared));
        }
        if (!valid || length > kMaxBodyLength) {
            tooLarge = valid;  // an invalid length leaves the body empty
        } else {
            body.resize(length);
            const std::size_t read = std::fread(body.data(), 1, length, stdin);
            body.resize(read);
        }
    }
    return CgiRequest(method, environment("PATH_INFO"), environment("SCRIPT_NAME"),
                      environment("QUERY_STRING"), body, environment("CONTENT_TYPE"),
                      environment("REMOTE_ADDR"), tooLarge);
}

CgiRequest::CgiRequest(std::string method, std::string path, std::string scriptName,
                       std::string queryString, std::string body, std::string contentType,
                       std::string remoteAddress, bool bodyTooLarge)
    : requestMethod(std::move(method)),
      pathInfo(std::move(path)),
      script(std::move(scriptName)),
      rawQueryLength(queryString.size()),
      remote(std::move(remoteAddress)),
      tooLargeBody(bodyTooLarge || body.size() > kMaxBodyLength)
{
    if (!queryTooLong()) {
        query = parseQueryString(queryString);
    }
    // "application/x-www-form-urlencoded", optionally followed by "; charset=...".
    const std::string expected = "application/x-www-form-urlencoded";
    formEncoded = contentType.compare(0, expected.size(), expected) == 0 &&
                  (contentType.size() == expected.size() || contentType[expected.size()] == ';');
    if (formEncoded && !tooLargeBody) {
        form = parseQueryString(body);
    }
}

std::optional<std::string> CgiRequest::formParam(const std::string& name) const
{
    const auto found = form.find(name);
    if (found == form.end()) {
        return std::nullopt;
    }
    return found->second;
}

std::optional<std::string> CgiRequest::queryParam(const std::string& name) const
{
    const auto found = query.find(name);
    if (found == query.end()) {
        return std::nullopt;
    }
    return found->second;
}

std::optional<std::string> urlDecode(const std::string& text)
{
    std::string decoded;
    decoded.reserve(text.size());
    for (std::size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        if (c == '+') {
            decoded += ' ';
        } else if (c == '%') {
            if (i + 2 >= text.size()) {  // needs two hex digits after '%'
                return std::nullopt;
            }
            const int high = hexValue(text[i + 1]);
            const int low = hexValue(text[i + 2]);
            if (high < 0 || low < 0) {
                return std::nullopt;
            }
            decoded += static_cast<char>(high * 16 + low);
            i += 2;
        } else {
            decoded += c;
        }
    }
    return decoded;
}

std::map<std::string, std::string> parseQueryString(const std::string& query)
{
    std::map<std::string, std::string> params;
    std::size_t start = 0;
    while (start <= query.size()) {
        std::size_t end = query.find('&', start);
        if (end == std::string::npos) {
            end = query.size();
        }
        const std::string pair = query.substr(start, end - start);
        const std::size_t equals = pair.find('=');
        if (!pair.empty() && equals != std::string::npos && equals > 0) {
            const auto key = urlDecode(pair.substr(0, equals));
            const auto value = urlDecode(pair.substr(equals + 1));
            if (key && value) {
                params.emplace(*key, *value);  // emplace keeps the first value
            }
        }
        start = end + 1;
    }
    return params;
}

std::optional<int> parsePositiveInt(const std::string& text)
{
    if (text.empty() || text.size() > 6) {
        return std::nullopt;
    }
    int value = 0;
    for (const char c : text) {
        if (c < '0' || c > '9') {
            return std::nullopt;
        }
        value = value * 10 + (c - '0');
    }
    if (value == 0) {
        return std::nullopt;
    }
    return value;
}
