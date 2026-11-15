#ifndef HTML_H
#define HTML_H

#include <string>
#include <string_view>

// Escapes text for HTML content and quoted attributes. Every value that
// comes from a user or the database goes through this before printing.
std::string escapeHtml(std::string_view text);

#endif // HTML_H
