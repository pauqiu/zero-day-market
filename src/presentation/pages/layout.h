#ifndef LAYOUT_H
#define LAYOUT_H

#include <string>

// Wraps a page body in the common document. `title` is plain text and is
// escaped here; `bodyHtml` must already be safe HTML.
std::string renderLayout(const std::string& title, const std::string& bodyHtml,
                         const std::string& scriptName);

std::string renderErrorBody(const std::string& heading, const std::string& message,
                            const std::string& scriptName);

#endif // LAYOUT_H
