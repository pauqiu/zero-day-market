#ifndef CATALOG_PAGE_H
#define CATALOG_PAGE_H

#include <string>

#include "application/catalog/catalog_item.h"
#include "presentation/templates/template_engine.h"

// Body of the catalog page (HU-01), from templates/catalog.html.
Html renderCatalogBody(const CatalogPage& page, const std::string& scriptName);

#endif // CATALOG_PAGE_H
