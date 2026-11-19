#ifndef MARIADB_OBJECT_HISTORY_REPOSITORY_H
#define MARIADB_OBJECT_HISTORY_REPOSITORY_H

#include "application/objects/object_history_repository.h"
#include "infrastructure/db/mariadb_connection.h"

class MariaDbObjectHistoryRepository : public ObjectHistoryRepository {
    public:
        explicit MariaDbObjectHistoryRepository(MariaDbConnection& connection);

        std::optional<std::string> findObjectName(int objectID) override;
        std::vector<TransferEntry> listTransfers(int objectID) override;

    private:
        MariaDbConnection& connection;
};

#endif // MARIADB_OBJECT_HISTORY_REPOSITORY_H
