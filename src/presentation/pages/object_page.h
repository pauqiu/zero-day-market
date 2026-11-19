#ifndef OBJECT_PAGE_H
#define OBJECT_PAGE_H

#include <string>

#include "application/objects/object_detail.h"
#include "presentation/templates/template_engine.h"

// Body of the object detail page (HU-02), from templates/object.html.
Html renderObjectBody(const ObjectDetail& detail, const std::string& scriptName);

#endif // OBJECT_PAGE_H
