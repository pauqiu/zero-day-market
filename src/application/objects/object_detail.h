#ifndef OBJECT_DETAIL_H
#define OBJECT_DETAIL_H

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// Read models for the object detail (HU-02). Times are UTC epoch seconds.

struct ObjectRecord {
    int objectID = 0;
    std::string name;
    std::string description;
    std::optional<std::string> ownerUsername;   // empty: initial inventory
    std::optional<std::int64_t> lastTransferAt; // empty: never transferred
};

struct DocumentSummary {
    int documentID = 0;
    std::string originalName;
    std::string mimeType;
    long sizeBytes = 0;
    std::int64_t uploadedAt = 0;
};

enum class Availability {
    InitialInventory,  // no owner yet: cannot be offered (RF-4.5)
    Blocked,           // transferred less than 30 minutes ago (RF-1.7)
    Available
};

struct ObjectDetail {
    ObjectRecord object;
    Availability availability = Availability::Available;
    std::optional<std::int64_t> blockedUntil;   // only when Blocked
    std::vector<DocumentSummary> documents;
};

#endif // OBJECT_DETAIL_H
