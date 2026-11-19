#ifndef GET_OBJECT_HISTORY_H
#define GET_OBJECT_HISTORY_H

#include <optional>

#include "application/objects/object_history.h"
#include "application/objects/object_history_repository.h"

// Use case HU-03: the public transfer history of one object (RF-1.10 to RF-1.13).
class GetObjectHistory {
    public:
        explicit GetObjectHistory(ObjectHistoryRepository& repository);

        // Empty when the object does not exist.
        std::optional<ObjectHistory> execute(int objectID);

    private:
        ObjectHistoryRepository& repository;
};

#endif // GET_OBJECT_HISTORY_H
