#include "application/catalog/list_catalog.h"

#include <algorithm>

ListCatalog::ListCatalog(CatalogRepository& repository)
    : repository(repository)
{
}

CatalogPage ListCatalog::execute(int requestedPage)
{
    CatalogPage result;
    result.totalObjects = repository.countObjects();

    const long pages = (result.totalObjects + kPageSize - 1) / kPageSize;
    result.totalPages = static_cast<int>(std::max(1L, pages));
    result.page = std::clamp(requestedPage, 1, result.totalPages);

    if (result.totalObjects > 0) {
        const long offset = static_cast<long>(result.page - 1) * kPageSize;
        result.items = repository.listObjects(kPageSize, offset);
    }
    return result;
}
