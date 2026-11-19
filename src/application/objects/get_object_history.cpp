#include "application/objects/get_object_history.h"

GetObjectHistory::GetObjectHistory(ObjectHistoryRepository& repository)
    : repository(repository)
{
}

std::optional<ObjectHistory> GetObjectHistory::execute(int objectID)
{
    std::optional<std::string> name = repository.findObjectName(objectID);
    if (!name) {
        return std::nullopt;
    }
    ObjectHistory history;
    history.objectID = objectID;
    history.objectName = std::move(*name);
    history.transfers = repository.listTransfers(objectID);
    return history;
}
