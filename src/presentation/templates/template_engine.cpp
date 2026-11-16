#include "presentation/templates/template_engine.h"

#include <algorithm>

#include "presentation/pages/html.h"
#include "presentation/templates/template_sources.h"

namespace {

struct Node {
    enum class Type { Text, Variable, RawVariable, Section, InvertedSection };

    Type type = Type::Text;
    std::string value;  // literal text, or the variable/section name
    std::vector<Node> children;
};

std::string_view trim(std::string_view text)
{
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) {
        return {};
    }
    const auto last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

std::string validName(std::string_view name)
{
    if (name.empty()) {
        throw TemplateError("empty name in template tag");
    }
    for (const char c : name) {
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                        (c >= '0' && c <= '9') || c == '_';
        if (!ok) {
            throw TemplateError("invalid name in template tag: " + std::string(name));
        }
    }
    return std::string(name);
}

void appendText(std::vector<Node>& nodes, std::string_view text)
{
    if (text.empty()) {
        return;
    }
    if (!nodes.empty() && nodes.back().type == Node::Type::Text) {
        nodes.back().value += text;
    } else {
        nodes.push_back({Node::Type::Text, std::string(text), {}});
    }
}

std::vector<Node> parse(std::string_view source)
{
    std::vector<Node> root;
    // Where new nodes go: the root, or the children of the open section.
    std::vector<std::vector<Node>*> targets{&root};
    std::vector<std::string> openSections;

    std::size_t pos = 0;
    while (pos < source.size()) {
        const std::size_t start = source.find("{{", pos);
        if (start == std::string_view::npos) {
            appendText(*targets.back(), source.substr(pos));
            break;
        }
        appendText(*targets.back(), source.substr(pos, start - pos));

        const bool triple = source.compare(start, 3, "{{{") == 0;
        const std::string_view open = triple ? "{{{" : "{{";
        const std::string_view close = triple ? "}}}" : "}}";
        const std::size_t end = source.find(close, start + open.size());
        if (end == std::string_view::npos) {
            throw TemplateError("unclosed template tag");
        }
        const std::string_view inner =
            trim(source.substr(start + open.size(), end - start - open.size()));
        pos = end + close.size();

        if (triple) {
            targets.back()->push_back({Node::Type::RawVariable, validName(inner), {}});
            continue;
        }
        if (inner.empty()) {
            throw TemplateError("empty template tag");
        }

        const char sigil = inner.front();

        // A section, closing or comment tag alone on its line produces no
        // output line: drop its indentation and the line break after it.
        if (sigil == '#' || sigil == '^' || sigil == '/' || sigil == '!') {
            const std::size_t lineStart = source.rfind('\n', start == 0 ? 0 : start - 1);
            const std::size_t indentFrom = lineStart == std::string_view::npos || start == 0
                                               ? 0
                                               : lineStart + 1;
            const std::size_t lineEnd = source.find('\n', pos);
            const std::size_t restTo = lineEnd == std::string_view::npos ? source.size() : lineEnd;
            const auto blank = [](std::string_view text) {
                return text.find_first_not_of(" \t\r") == std::string_view::npos;
            };
            if (blank(source.substr(indentFrom, start - indentFrom)) &&
                blank(source.substr(pos, restTo - pos))) {
                std::vector<Node>& nodes = *targets.back();
                const std::size_t indent = start - indentFrom;
                if (indent > 0 && !nodes.empty() && nodes.back().type == Node::Type::Text) {
                    std::string& text = nodes.back().value;
                    text.erase(text.size() - std::min(indent, text.size()));
                }
                pos = lineEnd == std::string_view::npos ? source.size() : lineEnd + 1;
            }
        }

        if (sigil == '!') {
            continue;  // comment
        }
        if (sigil == '#' || sigil == '^') {
            const std::string name = validName(trim(inner.substr(1)));
            const auto type = sigil == '#' ? Node::Type::Section : Node::Type::InvertedSection;
            targets.back()->push_back({type, name, {}});
            targets.push_back(&targets.back()->back().children);
            openSections.push_back(name);
        } else if (sigil == '/') {
            const std::string name = validName(trim(inner.substr(1)));
            if (openSections.empty() || openSections.back() != name) {
                throw TemplateError("unexpected closing tag: " + name);
            }
            openSections.pop_back();
            targets.pop_back();
        } else {
            targets.back()->push_back({Node::Type::Variable, validName(inner), {}});
        }
    }

    if (!openSections.empty()) {
        throw TemplateError("unclosed section: " + openSections.back());
    }
    return root;
}

using Scopes = std::vector<const TemplateContext*>;

// Inner scopes (list items) first, then the enclosing contexts.
const TemplateValue& lookup(const Scopes& scopes, const std::string& name)
{
    for (auto scope = scopes.rbegin(); scope != scopes.rend(); ++scope) {
        if (const TemplateValue* value = (*scope)->find(name)) {
            return *value;
        }
    }
    throw TemplateError("unknown template variable: " + name);
}

void render(const std::vector<Node>& nodes, Scopes& scopes, std::string& out)
{
    for (const Node& node : nodes) {
        switch (node.type) {
            case Node::Type::Text:
                out += node.value;
                break;

            case Node::Type::Variable: {
                const TemplateValue& value = lookup(scopes, node.value);
                if (value.kind != TemplateValue::Kind::Text) {
                    throw TemplateError("{{" + node.value + "}} needs a text value");
                }
                out += escapeHtml(value.text);
                break;
            }

            case Node::Type::RawVariable: {
                const TemplateValue& value = lookup(scopes, node.value);
                if (value.kind != TemplateValue::Kind::Html) {
                    throw TemplateError("{{{" + node.value + "}}} needs an Html value");
                }
                out += value.text;
                break;
            }

            case Node::Type::Section:
            case Node::Type::InvertedSection: {
                const TemplateValue& value = lookup(scopes, node.value);
                const bool inverted = node.type == Node::Type::InvertedSection;
                if (value.kind == TemplateValue::Kind::Flag) {
                    if (value.flag != inverted) {
                        render(node.children, scopes, out);
                    }
                } else if (value.kind == TemplateValue::Kind::List) {
                    if (inverted) {
                        if (value.items.empty()) {
                            render(node.children, scopes, out);
                        }
                    } else {
                        for (const TemplateContext& item : value.items) {
                            scopes.push_back(&item);
                            render(node.children, scopes, out);
                            scopes.pop_back();
                        }
                    }
                } else {
                    throw TemplateError("section " + node.value + " needs a flag or a list");
                }
                break;
            }
        }
    }
}

}  // namespace

TemplateContext& TemplateContext::setText(const std::string& name, std::string text)
{
    TemplateValue value;
    value.kind = TemplateValue::Kind::Text;
    value.text = std::move(text);
    put(name, std::move(value));
    return *this;
}

TemplateContext& TemplateContext::setNumber(const std::string& name, long number)
{
    return setText(name, std::to_string(number));
}

TemplateContext& TemplateContext::setFlag(const std::string& name, bool flag)
{
    TemplateValue value;
    value.kind = TemplateValue::Kind::Flag;
    value.flag = flag;
    put(name, std::move(value));
    return *this;
}

TemplateContext& TemplateContext::setList(const std::string& name, std::vector<TemplateContext> items)
{
    TemplateValue value;
    value.kind = TemplateValue::Kind::List;
    value.items = std::move(items);
    put(name, std::move(value));
    return *this;
}

TemplateContext& TemplateContext::setHtml(const std::string& name, const Html& html)
{
    TemplateValue value;
    value.kind = TemplateValue::Kind::Html;
    value.text = html.str();
    put(name, std::move(value));
    return *this;
}

const TemplateValue* TemplateContext::find(std::string_view name) const
{
    for (std::size_t i = 0; i < names.size(); ++i) {
        if (names[i] == name) {
            return &values[i];
        }
    }
    return nullptr;
}

void TemplateContext::put(const std::string& name, TemplateValue value)
{
    for (std::size_t i = 0; i < names.size(); ++i) {
        if (names[i] == name) {
            values[i] = std::move(value);
            return;
        }
    }
    names.push_back(name);
    values.push_back(std::move(value));
}

Html renderTemplate(std::string_view source, const TemplateContext& context)
{
    const std::vector<Node> nodes = parse(source);
    Scopes scopes{&context};
    std::string out;
    out.reserve(source.size() * 2);
    render(nodes, scopes, out);
    return Html(std::move(out));
}

Html renderNamedTemplate(std::string_view name, const TemplateContext& context)
{
    const auto source = templateSource(name);
    if (!source) {
        throw TemplateError("unknown template: " + std::string(name));
    }
    return renderTemplate(*source, context);
}
