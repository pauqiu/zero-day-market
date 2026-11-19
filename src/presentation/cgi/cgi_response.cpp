#include "presentation/cgi/cgi_response.h"

namespace {

const char* reasonPhrase(int status)
{
    switch (status) {
        case 200: return "OK";
        case 303: return "See Other";
        case 400: return "Bad Request";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 413: return "Content Too Large";
        case 414: return "URI Too Long";
        case 415: return "Unsupported Media Type";
        case 422: return "Unprocessable Content";
        default:  return "Internal Server Error";
    }
}

}  // namespace

void writeResponse(const CgiResponse& response, std::ostream& out, bool includeBody)
{
    out << "Status: " << response.status << ' ' << reasonPhrase(response.status) << "\r\n";
    out << "Content-Type: " << response.contentType << "\r\n";
    for (const auto& [name, value] : response.headers) {
        out << name << ": " << value << "\r\n";
    }
    out << "\r\n";
    if (includeBody) {
        out << response.body;
    }
    out.flush();
}
