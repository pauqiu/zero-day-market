#ifndef LAYOUT_H
#define LAYOUT_H

#include <optional>
#include <string>

#include "presentation/templates/template_engine.h"

// Complete document (templates/layout.html) around a rendered page body.
// `username` is set when the visitor has a full session (password and
// YubiKey): the header then offers "Mi cuenta" and "Cerrar sesión".
Html renderPage(const std::string& title, const Html& body, const std::string& scriptName,
                const std::optional<std::string>& username = std::nullopt);

// Body of an error page (templates/error.html).
Html renderErrorBody(const std::string& heading, const std::string& message,
                     const std::string& scriptName);

#endif // LAYOUT_H
