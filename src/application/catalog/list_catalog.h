#ifndef LIST_CATALOG_H
#define LIST_CATALOG_H

#include "application/catalog/catalog_item.h"
#include "application/catalog/catalog_repository.h"

// Use case HU-01: list the catalog one page at a time (RF-1.1, RF-1.5).
class ListCatalog {
    public:
        static constexpr int kPageSize = 20;

        explicit ListCatalog(CatalogRepository& repository);

        // A page outside the valid range is clamped to the nearest valid one.
        CatalogPage execute(int requestedPage);

    private:
        CatalogRepository& repository;
};

#endif // LIST_CATALOG_H
