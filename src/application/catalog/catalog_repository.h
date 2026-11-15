#ifndef CATALOG_REPOSITORY_H
#define CATALOG_REPOSITORY_H

#include <vector>

#include "application/catalog/catalog_item.h"

// Port: what the catalog use case needs from storage. Infrastructure
// implements it (MariaDbCatalogRepository); tests use a fake.
class CatalogRepository {
    public:
        virtual ~CatalogRepository() = default;

        virtual long countObjects() = 0;

        // Objects ordered by ID. limit > 0 and offset >= 0.
        virtual std::vector<CatalogItem> listObjects(int limit, long offset) = 0;
};

#endif // CATALOG_REPOSITORY_H
