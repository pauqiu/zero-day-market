// Unit tests with plain CTest (no third-party framework).
// Run: ctest --test-dir build --output-on-failure

#include <iostream>
#include <string>
#include <vector>

#include "application/catalog/list_catalog.h"
#include "application/objects/get_object_detail.h"
#include "domain/objects/transfer_rules.h"
#include "presentation/cgi/cgi_request.h"
#include "presentation/pages/catalog_page.h"
#include "presentation/pages/formatting.h"
#include "presentation/pages/html.h"
#include "presentation/pages/object_page.h"
#include "presentation/router.h"
#include "presentation/templates/template_engine.h"

namespace {

int failures = 0;

// assert() is disabled in Release builds (NDEBUG), so use our own check.
#define CHECK(condition)                                                     \
    do {                                                                     \
        if (!(condition)) {                                                  \
            std::cerr << __FILE__ << ':' << __LINE__ << ": " #condition "\n"; \
            ++failures;                                                      \
        }                                                                    \
    } while (false)

class FakeCatalogRepository : public CatalogRepository {
    public:
        long total = 0;
        int lastLimit = -1;
        long lastOffset = -1;

        long countObjects() override { return total; }

        std::vector<CatalogItem> listObjects(int limit, long offset) override
        {
            lastLimit = limit;
            lastOffset = offset;
            return {};
        }
};

void testEscapeHtml()
{
    CHECK(escapeHtml("<script>alert('x') & \"y\"</script>") ==
          "&lt;script&gt;alert(&#39;x&#39;) &amp; &quot;y&quot;&lt;/script&gt;");
    CHECK(escapeHtml("Máscara") == "Máscara");
}

void testUrlDecode()
{
    CHECK(urlDecode("a%20b+c") == std::optional<std::string>("a b c"));
    CHECK(!urlDecode("%"));
    CHECK(!urlDecode("%4"));
    CHECK(!urlDecode("%zz"));
}

void testParseQueryString()
{
    const auto params = parseQueryString("page=2&page=9&&=x&flag&q=a%26b");
    CHECK(params.at("page") == "2");   // first value wins
    CHECK(params.at("q") == "a&b");
    CHECK(params.count("flag") == 0);  // no '=' -> ignored
    CHECK(params.size() == 2);
}

void testParsePositiveInt()
{
    CHECK(parsePositiveInt("12") == std::optional<int>(12));
    CHECK(!parsePositiveInt("0"));
    CHECK(!parsePositiveInt("-1"));
    CHECK(!parsePositiveInt("+1"));
    CHECK(!parsePositiveInt("1e3"));
    CHECK(!parsePositiveInt("9999999"));
    CHECK(!parsePositiveInt(""));
}

void testListCatalogPagination()
{
    FakeCatalogRepository repository;
    ListCatalog listCatalog(repository);

    repository.total = 45;  // 3 pages of 20
    CatalogPage page = listCatalog.execute(2);
    CHECK(page.page == 2 && page.totalPages == 3);
    CHECK(repository.lastLimit == 20 && repository.lastOffset == 20);

    page = listCatalog.execute(99);  // clamped to the last page
    CHECK(page.page == 3 && repository.lastOffset == 40);

    repository.total = 0;
    repository.lastOffset = -1;
    page = listCatalog.execute(5);
    CHECK(page.page == 1 && page.totalPages == 1);
    CHECK(repository.lastOffset == -1);  // no listing query when empty
}

void testCatalogPageEscapesData()
{
    CatalogPage page;
    page.totalObjects = 1;
    page.items.push_back({7, "<b>Joya</b>", "a & b", std::nullopt});
    const std::string html = renderCatalogBody(page, "/app.cgi").str();
    CHECK(html.find("<b>Joya</b>") == std::string::npos);
    CHECK(html.find("&lt;b&gt;Joya&lt;/b&gt;") != std::string::npos);
    CHECK(html.find("Inventario del mercado") != std::string::npos);
    CHECK(html.find("/app.cgi/objeto?id=7") != std::string::npos);
}

bool throwsTemplateError(std::string_view source, const TemplateContext& context)
{
    try {
        renderTemplate(source, context);
    } catch (const TemplateError&) {
        return true;
    }
    return false;
}

void testTemplateEngine()
{
    TemplateContext context;
    context.setText("name", "<b>Joya</b> & 'co'").setFlag("yes", true).setFlag("no", false);

    // Text is always escaped.
    CHECK(renderTemplate("[{{name}}]", context).str() ==
          "[&lt;b&gt;Joya&lt;/b&gt; &amp; &#39;co&#39;]");
    CHECK(renderTemplate("{{ name }}", context).str() == renderTemplate("{{name}}", context).str());

    // Flags and inverted sections.
    CHECK(renderTemplate("{{#yes}}A{{/yes}}{{#no}}B{{/no}}{{^no}}C{{/no}}", context).str() == "AC");

    // Lists: one rendering per item; inner names shadow outer ones.
    std::vector<TemplateContext> items(2);
    items[0].setText("name", "uno");
    items[1].setText("name", "dos");
    TemplateContext listContext;
    listContext.setList("items", items).setText("sep", ",").setList("empty", {});
    CHECK(renderTemplate("{{#items}}{{name}}{{sep}}{{/items}}", listContext).str() == "uno,dos,");
    CHECK(renderTemplate("{{^empty}}vacío{{/empty}}{{#empty}}x{{/empty}}", listContext).str() == "vacío");

    // Raw insertion only for Html produced by the engine.
    TemplateContext page;
    page.setHtml("body", renderTemplate("<p>{{name}}</p>", context)).setText("text", "<i>");
    CHECK(renderTemplate("{{{body}}}", page).str() ==
          "<p>&lt;b&gt;Joya&lt;/b&gt; &amp; &#39;co&#39;</p>");
    CHECK(throwsTemplateError("{{{text}}}", page));  // Text cannot be inserted raw
    CHECK(throwsTemplateError("{{body}}", page));    // Html in {{ }} is a mistake

    // Mistakes are errors, not silent output.
    CHECK(throwsTemplateError("{{missing}}", context));
    CHECK(throwsTemplateError("{{#yes}}open", context));
    CHECK(throwsTemplateError("{{/yes}}", context));
    CHECK(throwsTemplateError("{{#yes}}{{/no}}", context));
    CHECK(throwsTemplateError("{{name", context));
    CHECK(throwsTemplateError("{{bad-name}}", context));
    CHECK(throwsTemplateError("{{#name}}x{{/name}}", context));  // text is not a section
    CHECK(renderTemplate("a{{! comment }}b", context).str() == "ab");

    // Section tags alone on their line leave no blank lines behind.
    CHECK(renderTemplate("a\n  {{#yes}}\n  b\n  {{/yes}}\nc", context).str() == "a\n  b\nc");
    CHECK(renderTemplate("{{#no}}\nx\n{{/no}}\nc", context).str() == "c");
    CHECK(renderTemplate("{{! note }}\nc", context).str() == "c");
    // Inline tags keep the surrounding text intact.
    CHECK(renderTemplate("<td>{{#yes}}si{{/yes}}</td>\n", context).str() == "<td>si</td>\n");
}

void testEmbeddedTemplatesRender()
{
    // Every page template must render with realistic data (catches typos).
    CatalogPage empty;
    CHECK(renderCatalogBody(empty, "/app.cgi").str().find("Todavía no hay objetos") !=
          std::string::npos);

    CatalogPage many;
    many.totalObjects = 45;
    many.totalPages = 3;
    many.page = 2;
    many.items.push_back({1, "Uno", "", std::string("ana")});
    const std::string html = renderCatalogBody(many, "/app.cgi").str();
    CHECK(html.find("45 objetos") != std::string::npos);
    CHECK(html.find("/app.cgi/catalogo?page=1") != std::string::npos);
    CHECK(html.find("/app.cgi/catalogo?page=3") != std::string::npos);
    CHECK(html.find("<td>ana</td>") != std::string::npos);

    CatalogPage one;
    one.totalObjects = 1;
    one.items.push_back({1, "Uno", "", std::nullopt});
    CHECK(renderCatalogBody(one, "/app.cgi").str().find("1 objeto en") != std::string::npos);
}

class FakeObjectDetailRepository : public ObjectDetailRepository {
    public:
        std::optional<ObjectRecord> object;
        std::vector<DocumentSummary> documents;
        int documentQueries = 0;

        std::optional<ObjectRecord> findObject(int) override { return object; }

        std::vector<DocumentSummary> listDocuments(int) override
        {
            ++documentQueries;
            return documents;
        }
};

void testTransferRule()
{
    const std::int64_t t = 1'800'000'000;
    CHECK(!transferBlockedUntil(std::nullopt, t));                     // never transferred
    CHECK(transferBlockedUntil(t, t) == std::optional<std::int64_t>(t + 1800));
    CHECK(transferBlockedUntil(t, t + 1799) == std::optional<std::int64_t>(t + 1800));
    CHECK(!transferBlockedUntil(t, t + 1800));                          // exactly 30 minutes
    CHECK(!transferBlockedUntil(t, t + 5000));
}

void testGetObjectDetail()
{
    const std::int64_t now = 1'800'000'000;
    FakeObjectDetailRepository repository;
    GetObjectDetail getDetail(repository, [now] { return now; });

    CHECK(!getDetail.execute(1));                    // not found
    CHECK(repository.documentQueries == 0);          // no document query if missing

    repository.object = ObjectRecord{1, "Atlas", "d", std::nullopt, std::nullopt};
    CHECK(getDetail.execute(1)->availability == Availability::InitialInventory);

    repository.object->ownerUsername = "ana";
    CHECK(getDetail.execute(1)->availability == Availability::Available);

    repository.object->lastTransferAt = now - 600;   // 10 minutes ago
    auto detail = getDetail.execute(1);
    CHECK(detail->availability == Availability::Blocked);
    CHECK(detail->blockedUntil == std::optional<std::int64_t>(now + 1200));

    repository.object->lastTransferAt = now - 1800;  // 30 minutes ago
    CHECK(getDetail.execute(1)->availability == Availability::Available);
}

void testFormatting()
{
    // 2026-11-19 20:35 UTC is 14:35 in Costa Rica (UTC-6).
    CHECK(formatLocalTime(1'795'120'500) == "19/11/2026 14:35");
    CHECK(formatLocalTime(1'795'060'800) == "18/11/2026 22:00");  // 04:00 UTC: previous day
    CHECK(formatSize(512) == "512 B");
    CHECK(formatSize(12'595) == "12.3 KB");
    CHECK(formatSize(1'258'291) == "1.2 MB");
    CHECK(documentTypeLabel("application/pdf") == "PDF");
    CHECK(documentTypeLabel("application/x-msdownload") == "Desconocido");
}

void testObjectPage()
{
    ObjectDetail detail;
    detail.object = ObjectRecord{7, "<b>Joya</b>", "Una \"joya\" & más", std::string("ana"),
                                 std::int64_t{1'795'120'500}};
    detail.availability = Availability::Blocked;
    detail.blockedUntil = 1'795'122'300;
    detail.documents.push_back({3, "cert<script>.pdf", "application/pdf", 2048, 1'795'120'500});

    const std::string html = renderObjectBody(detail, "/app.cgi").str();
    CHECK(html.find("<b>Joya</b>") == std::string::npos);
    CHECK(html.find("&lt;b&gt;Joya&lt;/b&gt;") != std::string::npos);
    CHECK(html.find("cert&lt;script&gt;.pdf") != std::string::npos);
    CHECK(html.find("Bloqueado hasta el 19/11/2026 15:05") != std::string::npos);
    CHECK(html.find("2.0 KB") != std::string::npos);
    CHECK(html.find("/app.cgi/historial?id=7") != std::string::npos);
    CHECK(html.find("<dd>ana</dd>") != std::string::npos);

    ObjectDetail inventory;
    inventory.object = ObjectRecord{8, "Atlas", "", std::nullopt, std::nullopt};
    inventory.availability = Availability::InitialInventory;
    const std::string html2 = renderObjectBody(inventory, "/app.cgi").str();
    CHECK(html2.find("Inventario del mercado") != std::string::npos);
    CHECK(html2.find("Nunca ha sido transferido") != std::string::npos);
    CHECK(html2.find("no tiene documentos") != std::string::npos);
    CHECK(html2.find("<table>") == std::string::npos);
}

void testRouter()
{
    UseCases useCases;
    int requested = 0;
    useCases.listCatalog = [&requested](int page) {
        requested = page;
        return CatalogPage{};
    };

    CHECK(route(CgiRequest("GET", "/catalogo", "/app.cgi", "page=3"), useCases).status == 200);
    CHECK(requested == 3);
    CHECK(route(CgiRequest("GET", "/catalogo", "/app.cgi", "page=abc"), useCases).status == 400);
    CHECK(route(CgiRequest("POST", "/catalogo", "/app.cgi", ""), useCases).status == 405);
    CHECK(route(CgiRequest("GET", "/nada", "/app.cgi", ""), useCases).status == 404);
    CHECK(route(CgiRequest("GET", "/", "/app.cgi", std::string(3000, 'a')), useCases).status == 414);

    int requestedObject = 0;
    useCases.getObjectDetail = [&requestedObject](int objectID) -> std::optional<ObjectDetail> {
        requestedObject = objectID;
        if (objectID != 5) {
            return std::nullopt;
        }
        ObjectDetail detail;
        detail.object = ObjectRecord{5, "Atlas", "", std::nullopt, std::nullopt};
        return detail;
    };
    CHECK(route(CgiRequest("GET", "/objeto", "/app.cgi", "id=5"), useCases).status == 200);
    CHECK(requestedObject == 5);
    CHECK(route(CgiRequest("GET", "/objeto", "/app.cgi", "id=6"), useCases).status == 404);
    requestedObject = 0;
    CHECK(route(CgiRequest("GET", "/objeto", "/app.cgi", ""), useCases).status == 400);
    CHECK(route(CgiRequest("GET", "/objeto", "/app.cgi", "id=abc"), useCases).status == 400);
    CHECK(route(CgiRequest("GET", "/objeto", "/app.cgi", "id=5%20OR%201=1"), useCases).status == 400);
    CHECK(route(CgiRequest("GET", "/objeto", "/app.cgi", "id=0"), useCases).status == 400);
    CHECK(requestedObject == 0);  // invalid IDs never reach the use case
    CHECK(route(CgiRequest("POST", "/objeto", "/app.cgi", "id=5"), useCases).status == 405);
}

}  // namespace

int main()
{
    testEscapeHtml();
    testUrlDecode();
    testParseQueryString();
    testParsePositiveInt();
    testListCatalogPagination();
    testCatalogPageEscapesData();
    testTransferRule();
    testGetObjectDetail();
    testFormatting();
    testObjectPage();
    testTemplateEngine();
    testEmbeddedTemplatesRender();
    testRouter();

    if (failures == 0) {
        std::cout << "All tests passed\n";
    }
    return failures == 0 ? 0 : 1;
}
