#ifndef FORMATTING_H
#define FORMATTING_H

#include <cstdint>
#include <string>

#include "application/common/local_time.h"  // formatLocalTime

// "512 B", "12.3 KB", "1.2 MB".
std::string formatSize(long bytes);

// Readable name of an allowed document type ("PDF", "PNG"...).
std::string documentTypeLabel(const std::string& mimeType);

#endif // FORMATTING_H
