#ifndef TRANSFER_RULES_H
#define TRANSFER_RULES_H

#include <cstdint>
#include <optional>

// Business rule of the market: an object traded or transferred at time t
// cannot be traded or transferred again until 30 minutes have passed.
// All times are seconds since the Unix epoch, in UTC.
constexpr std::int64_t kTransferCooldownSeconds = 30 * 60;

// The moment until which the object stays blocked, or nothing if the
// cooldown has already passed (or the object was never transferred).
std::optional<std::int64_t> transferBlockedUntil(std::optional<std::int64_t> lastTransferAt,
                                                 std::int64_t now);

#endif // TRANSFER_RULES_H
