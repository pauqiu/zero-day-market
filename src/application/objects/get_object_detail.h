#ifndef GET_OBJECT_DETAIL_H
#define GET_OBJECT_DETAIL_H

#include <cstdint>
#include <functional>
#include <optional>

#include "application/objects/object_detail.h"
#include "application/objects/object_detail_repository.h"

// Use case HU-02: the public detail of one object (RF-1.6 to RF-1.9).
class GetObjectDetail {
    public:
        // Current time as UTC epoch seconds; replaceable in tests.
        using Clock = std::function<std::int64_t()>;

        static std::int64_t systemClock();

        explicit GetObjectDetail(ObjectDetailRepository& repository, Clock now = systemClock);

        // Empty when the object does not exist.
        std::optional<ObjectDetail> execute(int objectID);

    private:
        ObjectDetailRepository& repository;
        Clock now;
};

#endif // GET_OBJECT_DETAIL_H
