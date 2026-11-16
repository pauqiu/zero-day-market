#include "presentation/pages/catalog_page.h"

#include <vector>

namespace {

std::string pageUrl(const std::string& scriptName, int page)
{
    return scriptName + "/catalogo?page=" + std::to_string(page);
}

}  // namespace

Html renderCatalogBody(const CatalogPage& page, const std::string& scriptName)
{
    std::vector<TemplateContext> objects;
    objects.reserve(page.items.size());
    for (const CatalogItem& item : page.items) {
        TemplateContext object;
        object.setNumber("id", item.objectID)
            .setText("name", item.name)
            .setText("description", item.shortDescription)
            .setText("detailUrl", scriptName + "/objeto?id=" + std::to_string(item.objectID))
            .setFlag("hasOwner", item.ownerUsername.has_value())
            .setText("owner", item.ownerUsername.value_or(""));
        objects.push_back(std::move(object));
    }

    TemplateContext context;
    context.setList("objects", std::move(objects))
        .setFlag("hasObjects", page.totalObjects > 0)
        .setNumber("totalObjects", page.totalObjects)
        .setFlag("oneObject", page.totalObjects == 1)
        .setFlag("showPager", page.totalPages > 1)
        .setNumber("page", page.page)
        .setNumber("totalPages", page.totalPages)
        .setFlag("hasPrevious", page.page > 1)
        .setText("previousUrl", pageUrl(scriptName, page.page - 1))
        .setFlag("hasNext", page.page < page.totalPages)
        .setText("nextUrl", pageUrl(scriptName, page.page + 1));
    return renderNamedTemplate("catalog", context);
}
