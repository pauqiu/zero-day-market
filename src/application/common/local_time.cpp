#include "application/common/local_time.h"

#include <cstdio>
#include <ctime>

namespace {

constexpr std::int64_t kCostaRicaOffsetSeconds = -6 * 3600;

}  // namespace

std::string formatLocalTime(std::int64_t utcSeconds)
{
    const std::time_t local = static_cast<std::time_t>(utcSeconds + kCostaRicaOffsetSeconds);
    std::tm parts{};
    if (gmtime_r(&local, &parts) == nullptr) {
        return "";
    }
    char buffer[64];  // room for any int value, as the compiler requires
    std::snprintf(buffer, sizeof buffer, "%02d/%02d/%04d %02d:%02d", parts.tm_mday,
                  parts.tm_mon + 1, parts.tm_year + 1900, parts.tm_hour, parts.tm_min);
    return buffer;
}
