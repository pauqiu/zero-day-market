#ifndef LOCAL_TIME_H
#define LOCAL_TIME_H

#include <cstdint>
#include <string>

// Formats a UTC epoch time in Costa Rica time (UTC-6, no daylight saving),
// as "19/11/2026 14:35". Used by pages and by email notifications.
std::string formatLocalTime(std::int64_t utcSeconds);

#endif // LOCAL_TIME_H
