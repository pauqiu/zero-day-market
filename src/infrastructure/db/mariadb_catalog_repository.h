#ifndef MARIADB_CATALOG_REPOSITORY_H
#define MARIADB_CATALOG_REPOSITORY_H

#include "application/catalog/catalog_repository.h"
#include "infrastructure/db/mariadb_connection.h"

class MariaDbCatalogRepository : public CatalogRepository {
    public:
        explicit MariaDbCatalogRepository(MariaDbConnection& connection);

        long countObjects() override;
        std::vector<CatalogItem> listObjects(int limit, long offset) override;

    private:
        MariaDbConnection& connection;
};

#endif // MARIADB_CATALOG_REPOSITORY_H
