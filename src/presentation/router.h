#ifndef ROUTER_H
#define ROUTER_H

#include <functional>

#include "application/catalog/catalog_item.h"
#include "presentation/cgi/cgi_request.h"
#include "presentation/cgi/cgi_response.h"

// Use cases the router can call. main.cpp wires them to the real
// infrastructure, so presentation never depends on MariaDB directly.
struct UseCases {
    std::function<CatalogPage(int requestedPage)> listCatalog;
};

CgiResponse route(const CgiRequest& request, const UseCases& useCases);

CgiResponse errorResponse(int status, const std::string& heading,
                          const std::string& message, const std::string& scriptName);

#endif // ROUTER_H
