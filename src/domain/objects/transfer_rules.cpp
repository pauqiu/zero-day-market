#include "domain/objects/transfer_rules.h"

std::optional<std::int64_t> transferBlockedUntil(std::optional<std::int64_t> lastTransferAt,
                                                 std::int64_t now)
{
    if (!lastTransferAt) {
        return std::nullopt;
    }
    const std::int64_t until = *lastTransferAt + kTransferCooldownSeconds;
    if (now < until) {
        return until;
    }
    return std::nullopt;
}
