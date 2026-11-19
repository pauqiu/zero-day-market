#ifndef ROUTER_H
#define ROUTER_H

#include <functional>
#include <optional>

#include "application/catalog/catalog_item.h"
#include "application/objects/object_detail.h"
#include "application/objects/object_history.h"
#include "presentation/cgi/cgi_request.h"
#include "presentation/cgi/cgi_response.h"

// Use cases the router can call. main.cpp wires them to the real
// infrastructure, so presentation never depends on MariaDB directly.
struct UseCases {
    std::function<CatalogPage(int requestedPage)> listCatalog;
    std::function<std::optional<ObjectDetail>(int objectID)> getObjectDetail;
    std::function<std::optional<ObjectHistory>(int objectID)> getObjectHistory;
};

CgiResponse route(const CgiRequest& request, const UseCases& useCases);

CgiResponse errorResponse(int status, const std::string& heading,
                          const std::string& message, const std::string& scriptName);

#endif // ROUTER_H
