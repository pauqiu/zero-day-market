#ifndef MARIADB_OBJECT_DETAIL_REPOSITORY_H
#define MARIADB_OBJECT_DETAIL_REPOSITORY_H

#include "application/objects/object_detail_repository.h"
#include "infrastructure/db/mariadb_connection.h"

class MariaDbObjectDetailRepository : public ObjectDetailRepository {
    public:
        explicit MariaDbObjectDetailRepository(MariaDbConnection& connection);

        std::optional<ObjectRecord> findObject(int objectID) override;
        std::vector<DocumentSummary> listDocuments(int objectID) override;

    private:
        MariaDbConnection& connection;
};

#endif // MARIADB_OBJECT_DETAIL_REPOSITORY_H
