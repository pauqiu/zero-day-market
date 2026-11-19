#include "presentation/pages/history_page.h"

#include <vector>

#include "presentation/pages/formatting.h"

namespace {

const char* operationLabel(TransferType type)
{
    switch (type) {
        case TransferType::InitialAllocation: return "Dotación inicial";
        case TransferType::Trade:             return "Trueque";
        case TransferType::Donation:          return "Donación";
    }
    return "";
}

}  // namespace

Html renderHistoryBody(const ObjectHistory& history, const std::string& scriptName)
{
    std::vector<TemplateContext> transfers;
    transfers.reserve(history.transfers.size());
    for (const TransferEntry& entry : history.transfers) {
        const bool hasExchange = entry.exchangedObjectID.has_value();
        TemplateContext row;
        row.setText("date", formatLocalTime(entry.transferredAt))
            .setText("operation", operationLabel(entry.type))
            .setFlag("hasFrom", entry.fromUsername.has_value())
            .setText("from", entry.fromUsername.value_or(""))
            .setText("to", entry.toUsername)
            .setFlag("hasExchange", hasExchange)
            .setText("exchangeName", entry.exchangedObjectName.value_or(""))
            .setText("exchangeUrl", hasExchange ? scriptName + "/objeto?id=" +
                                                  std::to_string(*entry.exchangedObjectID)
                                                : "");
        transfers.push_back(std::move(row));
    }

    const std::string id = std::to_string(history.objectID);
    TemplateContext context;
    context.setText("catalogUrl", scriptName + "/catalogo")
        .setText("detailUrl", scriptName + "/objeto?id=" + id)
        .setText("id", id)
        .setText("objectName", history.objectName)
        .setFlag("hasTransfers", !transfers.empty())
        .setList("transfers", std::move(transfers));
    return renderNamedTemplate("history", context);
}
