#include "presentation/pages/layout.h"

namespace {

// TODO: read from the Local entity / configuration instead of a constant.
constexpr const char* kCommercialName = "Jester's Chest";

}  // namespace

Html renderPage(const std::string& title, const Html& body, const std::string& scriptName)
{
    TemplateContext context;
    context.setText("title", title)
        .setText("commercialName", kCommercialName)
        .setText("homeUrl", scriptName + "/catalogo")
        .setText("registerUrl", scriptName + "/registro")
        .setHtml("body", body);
    return renderNamedTemplate("layout", context);
}

Html renderErrorBody(const std::string& heading, const std::string& message,
                     const std::string& scriptName)
{
    TemplateContext context;
    context.setText("heading", heading)
        .setText("message", message)
        .setText("homeUrl", scriptName + "/catalogo");
    return renderNamedTemplate("error", context);
}
