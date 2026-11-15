#ifndef CATALOG_ITEM_H
#define CATALOG_ITEM_H

#include <optional>
#include <string>
#include <vector>

// Read model for the public catalog (HU-01): what a visitor sees of each
// object. It is not the domain entity, because the catalog only reads.
struct CatalogItem {
    int objectID = 0;
    std::string name;
    std::string shortDescription;
    // Empty when the object belongs to the market's initial inventory.
    std::optional<std::string> ownerUsername;
};

struct CatalogPage {
    std::vector<CatalogItem> items;
    int page = 1;          // 1-based, always within [1, totalPages]
    int totalPages = 1;    // at least 1, even when there are no objects
    long totalObjects = 0;
};

#endif // CATALOG_ITEM_H
