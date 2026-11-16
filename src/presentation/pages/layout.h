#ifndef LAYOUT_H
#define LAYOUT_H

#include <string>

#include "presentation/templates/template_engine.h"

// Complete document (templates/layout.html) around a rendered page body.
Html renderPage(const std::string& title, const Html& body, const std::string& scriptName);

// Body of an error page (templates/error.html).
Html renderErrorBody(const std::string& heading, const std::string& message,
                     const std::string& scriptName);

#endif // LAYOUT_H
