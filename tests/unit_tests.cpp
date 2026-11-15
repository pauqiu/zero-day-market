// Unit tests with plain CTest (no third-party framework).
// Run: ctest --test-dir build --output-on-failure

#include <iostream>
#include <string>
#include <vector>

#include "application/catalog/list_catalog.h"
#include "presentation/cgi/cgi_request.h"
#include "presentation/pages/catalog_page.h"
#include "presentation/pages/html.h"
#include "presentation/router.h"

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
    const std::string html = renderCatalogBody(page, "/app.cgi");
    CHECK(html.find("<b>Joya</b>") == std::string::npos);
    CHECK(html.find("&lt;b&gt;Joya&lt;/b&gt;") != std::string::npos);
    CHECK(html.find("Inventario del mercado") != std::string::npos);
    CHECK(html.find("/app.cgi/objeto?id=7") != std::string::npos);
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
    testRouter();

    if (failures == 0) {
        std::cout << "All tests passed\n";
    }
    return failures == 0 ? 0 : 1;
}
