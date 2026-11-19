#ifndef OBJECT_HISTORY_H
#define OBJECT_HISTORY_H

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "domain/objects/transfer_type.h"

// Read models for the public transfer history (HU-03). Times are UTC epoch seconds.

struct TransferEntry {
    std::int64_t transferredAt = 0;
    TransferType type = TransferType::InitialAllocation;
    std::optional<std::string> fromUsername;     // empty: initial inventory
    std::string toUsername;
    std::optional<int> exchangedObjectID;        // trades only (RF-1.12)
    std::optional<std::string> exchangedObjectName;
};

struct ObjectHistory {
    int objectID = 0;
    std::string objectName;
    std::vector<TransferEntry> transfers;        // newest first (RF-1.10)
};

#endif // OBJECT_HISTORY_H
