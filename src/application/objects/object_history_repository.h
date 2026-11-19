#ifndef OBJECT_HISTORY_REPOSITORY_H
#define OBJECT_HISTORY_REPOSITORY_H

#include <optional>
#include <string>
#include <vector>

#include "application/objects/object_history.h"

// Port: what the history use case needs from storage. Read-only on purpose:
// the history is never modified from here (RF-1.13).
class ObjectHistoryRepository {
    public:
        virtual ~ObjectHistoryRepository() = default;

        virtual std::optional<std::string> findObjectName(int objectID) = 0;

        // Completed transfers of the object, newest first.
        virtual std::vector<TransferEntry> listTransfers(int objectID) = 0;
};

#endif // OBJECT_HISTORY_REPOSITORY_H
