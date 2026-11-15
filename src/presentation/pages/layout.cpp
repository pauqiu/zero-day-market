#include "presentation/pages/layout.h"

#include "presentation/pages/html.h"

namespace {

// TODO: read from the Local entity / configuration instead of a constant.
constexpr const char* kCommercialName = "Jester's Chest";

}  // namespace

std::string renderLayout(const std::string& title, const std::string& bodyHtml,
                         const std::string& scriptName)
{
    const std::string home = escapeHtml(scriptName + "/catalogo");
    std::string html;
    html += "<!DOCTYPE html>\n<html lang=\"es\">\n<head>\n";
    html += "<meta charset=\"utf-8\">\n";
    html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n";
    html += "<title>" + escapeHtml(title) + " | " + escapeHtml(kCommercialName) + "</title>\n";
    html += "<link rel=\"stylesheet\" href=\"/static/site.css\">\n";
    html += "</head>\n<body>\n";
    html += "<header class=\"site-header\"><a class=\"brand\" href=\"" + home + "\">" +
            escapeHtml(kCommercialName) + "</a>"
            "<span class=\"market\">Mercado el Día Cero</span></header>\n";
    html += "<main>\n" + bodyHtml + "</main>\n";
    html += "</body>\n</html>\n";
    return html;
}

std::string renderErrorBody(const std::string& heading, const std::string& message,
                            const std::string& scriptName)
{
    return "<h1>" + escapeHtml(heading) + "</h1>\n<p>" + escapeHtml(message) +
           "</p>\n<p><a href=\"" + escapeHtml(scriptName + "/catalogo") +
           "\">Ir al catálogo</a></p>\n";
}
