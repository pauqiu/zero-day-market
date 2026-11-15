#ifndef CGI_REQUEST_H
#define CGI_REQUEST_H

#include <map>
#include <optional>
#include <string>

// The parts of a CGI request the application uses, read once from the
// environment Apache provides.
class CgiRequest {
    public:
        static constexpr std::size_t kMaxQueryLength = 2048;

        static CgiRequest fromEnvironment();

        CgiRequest(std::string method, std::string path,
                   std::string scriptName, std::string queryString);

        const std::string& method() const { return requestMethod; }
        const std::string& path() const { return pathInfo; }
        const std::string& scriptName() const { return script; }
        bool queryTooLong() const { return rawQueryLength > kMaxQueryLength; }

        std::optional<std::string> queryParam(const std::string& name) const;

    private:
        std::string requestMethod;
        std::string pathInfo;
        std::string script;
        std::size_t rawQueryLength = 0;
        std::map<std::string, std::string> query;
};

// Decodes %XX and '+'. Returns nothing if an escape is malformed.
std::optional<std::string> urlDecode(const std::string& text);

// Splits "a=1&b=2". Malformed pairs are skipped; the first value wins.
std::map<std::string, std::string> parseQueryString(const std::string& query);

// Accepts only 1 to 6 decimal digits ("12"), nothing else ("+1", "1e3", "-1").
std::optional<int> parsePositiveInt(const std::string& text);

#endif // CGI_REQUEST_H
