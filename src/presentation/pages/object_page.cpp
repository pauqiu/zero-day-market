#include "presentation/pages/object_page.h"

#include <vector>

#include "presentation/pages/formatting.h"

Html renderObjectBody(const ObjectDetail& detail, const std::string& scriptName)
{
    const ObjectRecord& object = detail.object;

    std::vector<TemplateContext> documents;
    documents.reserve(detail.documents.size());
    for (const DocumentSummary& document : detail.documents) {
        TemplateContext row;
        row.setText("documentName", document.originalName)
            .setText("documentType", documentTypeLabel(document.mimeType))
            .setText("documentSize", formatSize(document.sizeBytes))
            .setText("uploadedAt", formatLocalTime(document.uploadedAt));
        documents.push_back(std::move(row));
    }

    const std::string id = std::to_string(object.objectID);
    TemplateContext context;
    context.setText("catalogUrl", scriptName + "/catalogo")
        .setText("historyUrl", scriptName + "/historial?id=" + id)
        .setText("id", id)
        .setText("name", object.name)
        .setText("description", object.description)
        .setFlag("hasOwner", object.ownerUsername.has_value())
        .setText("owner", object.ownerUsername.value_or(""))
        .setFlag("transferred", object.lastTransferAt.has_value())
        .setText("lastTransfer", object.lastTransferAt ? formatLocalTime(*object.lastTransferAt) : "")
        .setFlag("available", detail.availability == Availability::Available)
        .setFlag("blocked", detail.availability == Availability::Blocked)
        .setFlag("inventory", detail.availability == Availability::InitialInventory)
        .setText("blockedUntil", detail.blockedUntil ? formatLocalTime(*detail.blockedUntil) : "")
        .setFlag("hasDocuments", !documents.empty())
        .setList("documents", std::move(documents));
    return renderNamedTemplate("object", context);
}
