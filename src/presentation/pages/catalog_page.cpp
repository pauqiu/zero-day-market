#include "presentation/pages/catalog_page.h"

#include "presentation/pages/html.h"

namespace {

std::string pageLink(const std::string& scriptName, int page)
{
    return escapeHtml(scriptName + "/catalogo?page=" + std::to_string(page));
}

}  // namespace

std::string renderCatalogBody(const CatalogPage& page, const std::string& scriptName)
{
    std::string html = "<h1>Catálogo</h1>\n";

    if (page.totalObjects == 0) {
        html += "<p>Todavía no hay objetos en el catálogo.</p>\n";
        return html;
    }

    html += "<p class=\"summary\">" + std::to_string(page.totalObjects) +
            (page.totalObjects == 1 ? " objeto" : " objetos") + " en el mercado.</p>\n";

    html += "<div class=\"table-wrap\">\n<table>\n<thead><tr>"
            "<th scope=\"col\">ID</th><th scope=\"col\">Objeto</th>"
            "<th scope=\"col\">Descripción</th><th scope=\"col\">Propietario</th>"
            "</tr></thead>\n<tbody>\n";

    for (const CatalogItem& item : page.items) {
        const std::string id = std::to_string(item.objectID);
        const std::string detail = escapeHtml(scriptName + "/objeto?id=" + id);

        html += "<tr><td class=\"id\">" + id + "</td>";
        html += "<td><a href=\"" + detail + "\">" + escapeHtml(item.name) + "</a></td>";
        html += "<td>" + escapeHtml(item.shortDescription) + "</td>";
        if (item.ownerUsername) {
            html += "<td>" + escapeHtml(*item.ownerUsername) + "</td>";
        } else {
            html += "<td class=\"unowned\">Inventario del mercado</td>";
        }
        html += "</tr>\n";
    }
    html += "</tbody>\n</table>\n</div>\n";

    if (page.totalPages > 1) {
        html += "<nav class=\"pager\" aria-label=\"Páginas del catálogo\">";
        if (page.page > 1) {
            html += "<a href=\"" + pageLink(scriptName, page.page - 1) + "\">Anterior</a>";
        }
        html += "<span>Página " + std::to_string(page.page) + " de " +
                std::to_string(page.totalPages) + "</span>";
        if (page.page < page.totalPages) {
            html += "<a href=\"" + pageLink(scriptName, page.page + 1) + "\">Siguiente</a>";
        }
        html += "</nav>\n";
    }
    return html;
}
