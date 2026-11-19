#ifndef OBJECT_DETAIL_REPOSITORY_H
#define OBJECT_DETAIL_REPOSITORY_H

#include <optional>
#include <vector>

#include "application/objects/object_detail.h"

// Port: what the object detail use case needs from storage.
class ObjectDetailRepository {
    public:
        virtual ~ObjectDetailRepository() = default;

        virtual std::optional<ObjectRecord> findObject(int objectID) = 0;

        // Documents attached to the object, oldest first (metadata only).
        virtual std::vector<DocumentSummary> listDocuments(int objectID) = 0;
};

#endif // OBJECT_DETAIL_REPOSITORY_H
