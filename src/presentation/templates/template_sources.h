#ifndef TEMPLATE_SOURCES_H
#define TEMPLATE_SOURCES_H

#include <optional>
#include <string_view>

// Source of templates/<name>.html, embedded into the binary at build time by
// cmake/embed_templates.cmake. Templates cannot be changed on the server
// without a new build and deployment.
std::optional<std::string_view> templateSource(std::string_view name);

#endif // TEMPLATE_SOURCES_H
