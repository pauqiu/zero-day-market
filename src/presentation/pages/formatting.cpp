#include "presentation/pages/formatting.h"

#include <cstdio>

std::string formatSize(long bytes)
{
    char buffer[32];
    if (bytes < 1024) {
        std::snprintf(buffer, sizeof buffer, "%ld B", bytes);
    } else if (bytes < 1024L * 1024) {
        std::snprintf(buffer, sizeof buffer, "%.1f KB", static_cast<double>(bytes) / 1024);
    } else {
        std::snprintf(buffer, sizeof buffer, "%.1f MB", static_cast<double>(bytes) / (1024 * 1024));
    }
    return buffer;
}

std::string documentTypeLabel(const std::string& mimeType)
{
    if (mimeType == "application/pdf") return "PDF";
    if (mimeType == "image/png") return "PNG";
    if (mimeType == "image/jpeg") return "JPEG";
    if (mimeType == "image/svg+xml") return "SVG";
    if (mimeType == "text/html") return "HTML";
    return "Desconocido";
}
