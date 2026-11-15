#include "presentation/cgi/cgi_request.h"

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
    return CgiRequest(environment("REQUEST_METHOD"), environment("PATH_INFO"),
                      environment("SCRIPT_NAME"), environment("QUERY_STRING"));
}

CgiRequest::CgiRequest(std::string method, std::string path,
                       std::string scriptName, std::string queryString)
    : requestMethod(std::move(method)),
      pathInfo(std::move(path)),
      script(std::move(scriptName)),
      rawQueryLength(queryString.size())
{
    if (!queryTooLong()) {
        query = parseQueryString(queryString);
    }
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
