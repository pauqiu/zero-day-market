#include "application/objects/get_object_detail.h"

#include <chrono>

#include "domain/objects/transfer_rules.h"

std::int64_t GetObjectDetail::systemClock()
{
    using namespace std::chrono;
    return duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
}

GetObjectDetail::GetObjectDetail(ObjectDetailRepository& repository, Clock now)
    : repository(repository), now(std::move(now))
{
}

std::optional<ObjectDetail> GetObjectDetail::execute(int objectID)
{
    std::optional<ObjectRecord> object = repository.findObject(objectID);
    if (!object) {
        return std::nullopt;
    }

    ObjectDetail detail;
    detail.object = std::move(*object);

    if (!detail.object.ownerUsername) {
        detail.availability = Availability::InitialInventory;
    } else if (const auto until = transferBlockedUntil(detail.object.lastTransferAt, now())) {
        detail.availability = Availability::Blocked;
        detail.blockedUntil = until;
    } else {
        detail.availability = Availability::Available;
    }

    detail.documents = repository.listDocuments(objectID);
    return detail;
}
