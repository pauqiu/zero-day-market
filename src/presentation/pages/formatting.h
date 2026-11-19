#ifndef FORMATTING_H
#define FORMATTING_H

#include <cstdint>
#include <string>

// Formats a UTC epoch time in Costa Rica time (UTC-6, no daylight saving),
// as "19/11/2026 14:35".
std::string formatLocalTime(std::int64_t utcSeconds);

// "512 B", "12.3 KB", "1.2 MB".
std::string formatSize(long bytes);

// Readable name of an allowed document type ("PDF", "PNG"...).
std::string documentTypeLabel(const std::string& mimeType);

#endif // FORMATTING_H
