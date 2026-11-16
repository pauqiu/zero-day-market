#ifndef TEMPLATE_ENGINE_H
#define TEMPLATE_ENGINE_H

// Minimal template engine (a subset of Mustache syntax):
//
//   {{name}}               text value, always HTML-escaped
//   {{{name}}}             HTML value, inserted as is (only Html values)
//   {{#name}}...{{/name}}  flag: renders if true; list: renders once per item
//   {{^name}}...{{/name}}  renders if the flag is false or the list is empty
//   {{! comment }}         ignored
//
// Safety rules enforced here, not left to each page:
//   * Every {{name}} is escaped; there is no way to print raw text with it.
//   * {{{name}}} only accepts Html values, and Html can only be created by
//     rendering a template. Text from users or the database can never reach
//     the page unescaped.
//   * Unknown variables, wrong value types and malformed templates throw
//     TemplateError instead of silently printing nothing.
// Attributes must always be quoted in templates: href="{{url}}".

#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

class TemplateError : public std::runtime_error {
    public:
        using std::runtime_error::runtime_error;
};

class TemplateContext;

// HTML produced by the engine. It cannot be built from an arbitrary string.
class Html {
    public:
        const std::string& str() const { return content; }

    private:
        explicit Html(std::string content) : content(std::move(content)) {}

        std::string content;

        friend Html renderTemplate(std::string_view source, const TemplateContext& context);
};

struct TemplateValue;

// Named values for one template (or for one item of a list).
class TemplateContext {
    public:
        TemplateContext& setText(const std::string& name, std::string text);
        TemplateContext& setNumber(const std::string& name, long number);
        TemplateContext& setFlag(const std::string& name, bool flag);
        TemplateContext& setList(const std::string& name, std::vector<TemplateContext> items);
        TemplateContext& setHtml(const std::string& name, const Html& html);

        const TemplateValue* find(std::string_view name) const;

    private:
        void put(const std::string& name, TemplateValue value);

        std::vector<std::string> names;
        std::vector<TemplateValue> values;
};

struct TemplateValue {
    enum class Kind { Text, Html, Flag, List };

    Kind kind = Kind::Text;
    std::string text;                    // Text and Html
    bool flag = false;                   // Flag
    std::vector<TemplateContext> items;  // List
};

// Renders template source text.
Html renderTemplate(std::string_view source, const TemplateContext& context);

// Renders a template embedded at build time from templates/<name>.html.
Html renderNamedTemplate(std::string_view name, const TemplateContext& context);

#endif // TEMPLATE_ENGINE_H
