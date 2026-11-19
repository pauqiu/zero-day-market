#ifndef HISTORY_PAGE_H
#define HISTORY_PAGE_H

#include <string>

#include "application/objects/object_history.h"
#include "presentation/templates/template_engine.h"

// Body of the transfer history page (HU-03), from templates/history.html.
Html renderHistoryBody(const ObjectHistory& history, const std::string& scriptName);

#endif // HISTORY_PAGE_H
