#ifndef CGI_RESPONSE_H
#define CGI_RESPONSE_H

#include <ostream>
#include <string>
#include <utility>
#include <vector>

struct CgiResponse {
    int status = 200;
    std::string contentType = "text/html; charset=utf-8";
    std::vector<std::pair<std::string, std::string>> headers;
    std::string body;
};

// Writes the response in CGI format. For HEAD requests the body is omitted.
void writeResponse(const CgiResponse& response, std::ostream& out, bool includeBody);

#endif // CGI_RESPONSE_H
