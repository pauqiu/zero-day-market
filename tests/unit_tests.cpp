// Unit tests with plain CTest (no third-party framework).
// Run: ctest --test-dir build --output-on-failure

#include <yubikey.h>

#include <array>
#include <cstring>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "application/accounts/register_user.h"
#include "application/auth/authentication.h"
#include "application/catalog/list_catalog.h"
#include "application/objects/get_object_detail.h"
#include "application/objects/get_object_history.h"
#include "domain/objects/transfer_rules.h"
#include "domain/users/account_rules.h"
#include "infrastructure/crypto/aes_gcm_secret_box.h"
#include "infrastructure/crypto/random_session_tokens.h"
#include "infrastructure/crypto/scrypt_password_hasher.h"
#include "infrastructure/yubikey/libyubikey_otp_decoder.h"
#include "presentation/cgi/cgi_request.h"
#include "presentation/pages/catalog_page.h"
#include "presentation/cgi/session_cookie.h"
#include "presentation/pages/formatting.h"
#include "presentation/pages/history_page.h"
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

class FakeObjectHistoryRepository : public ObjectHistoryRepository {
    public:
        std::optional<std::string> name;
        std::vector<TransferEntry> transfers;
        int transferQueries = 0;

        std::optional<std::string> findObjectName(int) override { return name; }

        std::vector<TransferEntry> listTransfers(int) override
        {
            ++transferQueries;
            return transfers;
        }
};

void testGetObjectHistory()
{
    FakeObjectHistoryRepository repository;
    GetObjectHistory getHistory(repository);

    CHECK(!getHistory.execute(4));                 // unknown object
    CHECK(repository.transferQueries == 0);

    repository.name = "Atlas";
    repository.transfers.push_back({1, TransferType::Donation, std::string("ana"), "luis",
                                    std::nullopt, std::nullopt});
    const auto history = getHistory.execute(4);
    CHECK(history && history->objectID == 4 && history->objectName == "Atlas");
    CHECK(history->transfers.size() == 1);
}

void testHistoryPage()
{
    ObjectHistory history;
    history.objectID = 9;
    history.objectName = "<i>Atlas</i>";
    history.transfers.push_back({1'795'120'500, TransferType::Trade, std::string("ana"), "luis",
                                 12, std::string("Mapa <b>raro</b>")});
    history.transfers.push_back({1'795'000'000, TransferType::Donation, std::string("luis"), "ana",
                                 std::nullopt, std::nullopt});
    history.transfers.push_back({1'794'900'000, TransferType::InitialAllocation, std::nullopt,
                                 "luis", std::nullopt, std::nullopt});

    const std::string html = renderHistoryBody(history, "/app.cgi").str();
    CHECK(html.find("<i>Atlas</i>") == std::string::npos);
    CHECK(html.find("&lt;i&gt;Atlas&lt;/i&gt;") != std::string::npos);
    CHECK(html.find("Trueque") != std::string::npos);
    CHECK(html.find("Donación") != std::string::npos);
    CHECK(html.find("Dotación inicial") != std::string::npos);
    CHECK(html.find("Inventario del mercado") != std::string::npos);       // initial allocation
    CHECK(html.find("/app.cgi/objeto?id=12") != std::string::npos);       // RF-1.12
    CHECK(html.find("Mapa &lt;b&gt;raro&lt;/b&gt;") != std::string::npos);
    CHECK(html.find("19/11/2026 14:35") != std::string::npos);
    CHECK(html.find("<form") == std::string::npos);                       // RF-1.13: read-only
    // Order is kept as given by the repository (newest first).
    CHECK(html.find("Trueque") < html.find("Donación"));
    CHECK(html.find("Donación") < html.find("Dotación inicial"));

    ObjectHistory empty;
    empty.objectID = 3;
    empty.objectName = "Herbario";
    const std::string html2 = renderHistoryBody(empty, "/app.cgi").str();
    CHECK(html2.find("todavía no ha sido transferido") != std::string::npos);
    CHECK(html2.find("<table>") == std::string::npos);
}

// ---------------------------------------------------------------- accounts

const std::string kTestAesKey = "00112233445566778899aabbccddeeff";
const std::string kTestPublicID = "cccccbcdefgh";

// An OTP as the YubiKey with that AES key would type it.
std::string makeOtp(const std::string& aesKeyHex, const std::string& publicID,
                    std::uint16_t counter, std::uint8_t sessionUse)
{
    char key[YUBIKEY_KEY_SIZE + 1];
    yubikey_hex_decode(key, aesKeyHex.c_str(), sizeof key);  // size of the destination
    yubikey_token_st token{};
    const unsigned char uid[YUBIKEY_UID_SIZE] = {0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};
    std::memcpy(token.uid, uid, sizeof uid);
    token.ctr = counter;
    token.use = sessionUse;
    token.rnd = 0x4242;
    // A YubiKey stores the complement of the CRC-16 of the first 14 bytes;
    // yubikey_generate does not compute it, so it is set here.
    token.crc = static_cast<std::uint16_t>(
        ~yubikey_crc16(reinterpret_cast<const std::uint8_t*>(&token), YUBIKEY_BLOCK_SIZE - 2));
    char block[2 * YUBIKEY_BLOCK_SIZE + 1];
    yubikey_generate(&token, reinterpret_cast<const std::uint8_t*>(key), block);
    return publicID + block;
}

void testAccountRules()
{
    CHECK(isValidUsername("ana_maria-2"));
    CHECK(!isValidUsername("ab"));
    CHECK(!isValidUsername(std::string(33, 'a')));
    CHECK(!isValidUsername("ana maria"));
    CHECK(!isValidUsername("ana<b>"));
    CHECK(!isValidUsername("ana\nBcc"));

    CHECK(normalizeEmail("Ana.Perez@NAC.UCR.ac.cr") == std::optional<std::string>("ana.perez@nac.ucr.ac.cr"));
    CHECK(!normalizeEmail("ana@localhost"));             // needs a dot in the domain
    CHECK(!normalizeEmail("ana@@b.com"));
    CHECK(!normalizeEmail("a b@c.com"));
    CHECK(!normalizeEmail("ana@b.com\r\nBcc: x@y.com"));  // header injection
    CHECK(!normalizeEmail(".ana@b.com"));
    CHECK(!normalizeEmail("ana..p@b.com"));
    CHECK(!normalizeEmail("ana@-b.com"));
    CHECK(!normalizeEmail("\"ana\"@b.com"));

    CHECK(checkPasswordPolicy("Corta1!") == PasswordProblem::TooShort);
    CHECK(checkPasswordPolicy("solominusculas") == PasswordProblem::TooFewCharacterTypes);
    CHECK(checkPasswordPolicy("minusculasymas123") == PasswordProblem::TooFewCharacterTypes);
    CHECK(checkPasswordPolicy("Minusculas y 123") == PasswordProblem::None);
    CHECK(checkPasswordPolicy(std::string(129, 'a') + "A1") == PasswordProblem::TooLong);
}

void testScrypt()
{
    ScryptPasswordHasher hasher(ScryptPasswordHasher::Parameters{10, 8, 1});  // cheap for tests
    const std::string stored = hasher.hash("Contraseña segura 1");
    CHECK(stored.rfind("scrypt$10$8$1$", 0) == 0);
    CHECK(stored.find("Contraseña") == std::string::npos);
    CHECK(hasher.verify("Contraseña segura 1", stored));
    CHECK(!hasher.verify("Contraseña segura 2", stored));
    CHECK(hasher.hash("Contraseña segura 1") != stored);  // a new salt every time

    std::string tampered = stored;
    tampered.back() = tampered.back() == '0' ? '1' : '0';
    CHECK(!hasher.verify("Contraseña segura 1", tampered));
    // Parameters outside the accepted range are refused without computing.
    CHECK(!hasher.verify("x", "scrypt$30$8$1$" + std::string(32, 'a') + "$" + std::string(64, 'b')));
    CHECK(!hasher.verify("x", "md5$abc"));
    ScryptPasswordHasher production;  // the real parameters are valid
}

void testSecretBox()
{
    std::array<unsigned char, 32> key{};
    key.fill(7);
    AesGcmSecretBox box = AesGcmSecretBox::fromKey(key);
    const std::vector<unsigned char> secret = {1, 2, 3, 4, 5, 6};

    const auto sealed = box.seal(secret, "yubikey-private-id:cccccbcdefgh");
    CHECK(sealed.size() == 1 + 12 + secret.size() + 16);
    CHECK(box.open(sealed, "yubikey-private-id:cccccbcdefgh") == std::optional(secret));
    CHECK(!box.open(sealed, "yubikey-aes-key:cccccbcdefgh"));      // other column
    CHECK(!box.open(sealed, "yubikey-private-id:cccccbcdefgi"));   // other row
    CHECK(box.seal(secret, "c") != box.seal(secret, "c"));          // random nonce

    auto altered = sealed;
    altered[14] ^= 0x01;
    CHECK(!box.open(altered, "yubikey-private-id:cccccbcdefgh"));
    CHECK(!box.open(std::vector<unsigned char>(sealed.begin(), sealed.begin() + 20), "c"));

    key.fill(8);
    AesGcmSecretBox other = AesGcmSecretBox::fromKey(key);
    CHECK(!other.open(sealed, "yubikey-private-id:cccccbcdefgh"));  // another master key
}

void testOtpDecoder()
{
    LibyubikeyOtpDecoder decoder;
    const std::string otp = makeOtp(kTestAesKey, kTestPublicID, 5, 3);
    CHECK(otp.size() == 44);

    const auto decoded = decoder.decode(otp, kTestAesKey);
    CHECK(decoded.has_value());
    CHECK(decoded && decoded->publicID == kTestPublicID);
    CHECK(decoded && decoded->counter == 5 && decoded->sessionUse == 3);
    CHECK(decoded && decoded->privateID == std::vector<unsigned char>({0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f}));

    CHECK(!decoder.decode(otp, "ffeeddccbbaa99887766554433221100"));  // wrong AES key
    std::string altered = otp;
    altered[20] = altered[20] == 'c' ? 'd' : 'c';
    CHECK(!decoder.decode(altered, kTestAesKey));                       // altered block
    CHECK(!decoder.decode(otp.substr(0, 43), kTestAesKey));             // wrong length
    CHECK(!decoder.decode(otp + "c", kTestAesKey));
    CHECK(!decoder.decode(std::string(44, 'a'), kTestAesKey));          // not modhex
    CHECK(!decoder.decode(otp, "zz112233445566778899aabbccddeeff"));    // not hex
}

class FakeAccounts : public AccountRepository {
    public:
        CreateAccountOutcome outcome = CreateAccountOutcome::Created;
        std::vector<NewAccount> created;
        std::vector<std::string> incidents;

        CreateAccountResult createAccount(const NewAccount& account) override
        {
            created.push_back(account);
            return {outcome, outcome == CreateAccountOutcome::Created ? 41 : 0};
        }

        void recordIncident(std::string_view category, std::string_view, std::string_view ip,
                            std::string_view details) override
        {
            incidents.push_back(std::string(category) + "|" + std::string(ip) + "|" +
                                std::string(details));
        }
};

class FakeHasher : public PasswordHasher {
    public:
        std::string hash(std::string_view) override { return "scrypt$fake"; }
        bool verify(std::string_view, std::string_view) override { return false; }
};

class FakeBox : public SecretBox {
    public:
        std::vector<std::string> contexts;

        std::vector<unsigned char> seal(const std::vector<unsigned char>& plaintext,
                                        std::string_view context) override
        {
            contexts.emplace_back(context);
            std::vector<unsigned char> out(plaintext.size(), 0xEE);  // never the plaintext
            return out;
        }

        std::optional<std::vector<unsigned char>> open(const std::vector<unsigned char>&,
                                                       std::string_view) override
        {
            return std::nullopt;
        }
};

RegistrationRequest validRegistration()
{
    RegistrationRequest r;
    r.username = "colombina";
    r.email = "Colombina@NAC.UCR.ac.cr";
    r.password = "Mascara bicolor 2026";
    r.passwordConfirmation = r.password;
    r.aesKey = "00112233445566778899AABBCCDDEEFF";  // uppercase is accepted
    r.otp = makeOtp(kTestAesKey, kTestPublicID, 9, 2);
    r.clientIP = "172.24.0.15";
    return r;
}

bool hasError(const RegistrationResult& result, RegistrationError error)
{
    for (const RegistrationError e : result.errors) {
        if (e == error) {
            return true;
        }
    }
    return false;
}

void testRegisterUser()
{
    FakeAccounts accounts;
    FakeHasher hasher;
    FakeBox box;
    LibyubikeyOtpDecoder decoder;
    RegisterUser registerUser(accounts, hasher, box, decoder);

    // Success: secrets reach storage only protected.
    RegistrationResult ok = registerUser.execute(validRegistration());
    CHECK(ok.success && ok.userID == 41 && ok.username == "colombina");
    CHECK(accounts.created.size() == 1);
    const NewAccount& account = accounts.created.front();
    CHECK(account.email == "colombina@nac.ucr.ac.cr");
    CHECK(account.passwordHash == "scrypt$fake");
    CHECK(account.publicID == kTestPublicID);
    CHECK(account.counter == 9 && account.sessionUse == 2);
    CHECK(account.aesKeySealed == std::vector<unsigned char>(16, 0xEE));
    CHECK(account.privateIDSealed == std::vector<unsigned char>(6, 0xEE));
    CHECK(box.contexts.size() == 2 && box.contexts[0] == "yubikey-aes-key:" + kTestPublicID &&
          box.contexts[1] == "yubikey-private-id:" + kTestPublicID);
    CHECK(account.clientIP == "172.24.0.15");
    CHECK(accounts.incidents.empty());

    // Format errors are all reported at once, and nothing is stored.
    accounts.created.clear();
    RegistrationRequest bad;
    bad.username = "a b";
    bad.email = "nope";
    bad.password = "short";
    bad.passwordConfirmation = "other";
    bad.aesKey = "123";
    bad.otp = "xyz";
    RegistrationResult invalid = registerUser.execute(bad);
    CHECK(!invalid.success && invalid.errors.size() == 6);
    CHECK(hasError(invalid, RegistrationError::InvalidUsername) &&
          hasError(invalid, RegistrationError::InvalidEmail) &&
          hasError(invalid, RegistrationError::PasswordTooShort) &&
          hasError(invalid, RegistrationError::PasswordMismatch) &&
          hasError(invalid, RegistrationError::InvalidAesKeyFormat) &&
          hasError(invalid, RegistrationError::InvalidOtpFormat));
    CHECK(accounts.created.empty() && accounts.incidents.empty());

    // A wrong AES key: generic rejection, an incident, nothing stored.
    RegistrationRequest wrongKey = validRegistration();
    wrongKey.aesKey = "ffeeddccbbaa99887766554433221100";
    RegistrationResult rejected = registerUser.execute(wrongKey);
    CHECK(!rejected.success && hasError(rejected, RegistrationError::YubikeyRejected));
    CHECK(accounts.created.empty());
    CHECK(accounts.incidents.size() == 1 &&
          accounts.incidents[0] == "validacion|172.24.0.15|public_id=" + kTestPublicID);

    // Duplicates detected by the database.
    accounts.outcome = CreateAccountOutcome::UsernameTaken;
    CHECK(hasError(registerUser.execute(validRegistration()), RegistrationError::UsernameTaken));
    accounts.outcome = CreateAccountOutcome::EmailTaken;
    CHECK(hasError(registerUser.execute(validRegistration()), RegistrationError::EmailTaken));
    accounts.outcome = CreateAccountOutcome::YubikeyTaken;
    accounts.incidents.clear();
    CHECK(hasError(registerUser.execute(validRegistration()), RegistrationError::YubikeyRejected));
    CHECK(accounts.incidents.size() == 1);
}

void testFormRequests()
{
    CgiRequest form("POST", "/registro", "/app.cgi", "", "username=ana&otp=abc%2Bd&x=1",
                    "application/x-www-form-urlencoded; charset=UTF-8", "10.0.0.1");
    CHECK(form.hasFormBody() && !form.bodyTooLarge());
    CHECK(form.formParam("username") == std::optional<std::string>("ana"));
    CHECK(form.formParam("otp") == std::optional<std::string>("abc+d"));
    CHECK(!form.formParam("password"));
    CHECK(form.remoteAddress() == "10.0.0.1");

    CgiRequest json("POST", "/registro", "/app.cgi", "", "username=ana", "application/json");
    CHECK(!json.hasFormBody() && !json.formParam("username"));
    CgiRequest prefix("POST", "/registro", "/app.cgi", "", "username=ana",
                      "application/x-www-form-urlencodedXYZ");
    CHECK(!prefix.hasFormBody());

    CgiRequest big("POST", "/registro", "/app.cgi", "", std::string(CgiRequest::kMaxBodyLength + 1, 'a'),
                   "application/x-www-form-urlencoded");
    CHECK(big.bodyTooLarge() && !big.formParam("a"));
}

bool hasHeader(const CgiResponse& response, const std::string& name, const std::string& value)
{
    for (const auto& [n, v] : response.headers) {
        if (n == name && v == value) {
            return true;
        }
    }
    return false;
}

void testRegistrationRoutes()
{
    UseCases useCases;
    RegistrationRequest received;
    bool succeed = false;
    useCases.registerUser = [&](RegistrationRequest request) {
        received = request;
        RegistrationResult result;
        if (succeed) {
            result.success = true;
            result.username = request.username;
        } else {
            result.errors = {RegistrationError::YubikeyRejected};
        }
        return result;
    };

    const CgiResponse formPage = route(CgiRequest("GET", "/registro", "/app.cgi", ""), useCases);
    CHECK(formPage.status == 200 && hasHeader(formPage, "Cache-Control", "no-store"));
    CHECK(formPage.body.find("method=\"post\"") != std::string::npos);

    const std::string body =
        "username=%3Cb%3Eana&email=ana%40b.com&password=Secreta+12345&password_confirmation=Secreta+12345"
        "&aes_key=00112233445566778899aabbccddeeff&otp=cccccbcdefghtest";
    const std::string formType = "application/x-www-form-urlencoded";

    const CgiResponse failed =
        route(CgiRequest("POST", "/registro", "/app.cgi", "", body, formType, "10.1.1.1"), useCases);
    CHECK(received.username == "<b>ana" && received.password == "Secreta 12345" &&
          received.aesKey == "00112233445566778899aabbccddeeff" && received.clientIP == "10.1.1.1");
    CHECK(failed.status == 422 && hasHeader(failed, "Cache-Control", "no-store"));
    CHECK(failed.body.find("&lt;b&gt;ana") != std::string::npos);        // username escaped
    CHECK(failed.body.find("ana@b.com") != std::string::npos);           // email written back
    CHECK(failed.body.find("Secreta") == std::string::npos);             // never the password
    CHECK(failed.body.find("00112233445566778899aabbccddeeff") == std::string::npos);
    CHECK(failed.body.find("cccccbcdefghtest") == std::string::npos);
    CHECK(failed.body.find("no se puede registrar") != std::string::npos);

    succeed = true;
    const CgiResponse done =
        route(CgiRequest("POST", "/registro", "/app.cgi", "", body, formType, "10.1.1.1"), useCases);
    CHECK(done.status == 200 && done.body.find("Cuenta creada") != std::string::npos);

    CHECK(route(CgiRequest("PUT", "/registro", "/app.cgi", ""), useCases).status == 405);
    CHECK(route(CgiRequest("POST", "/registro", "/app.cgi", "", "{}", "application/json"), useCases)
              .status == 415);
    CHECK(route(CgiRequest("POST", "/registro", "/app.cgi", "", "", formType, "", true), useCases)
              .status == 413);
    CHECK(route(CgiRequest("POST", "/catalogo", "/app.cgi", "", body, formType), useCases).status == 405);
}


// ------------------------------------------------------------------- login

// In-memory AuthRepository with the same semantics as the SQL statements.
class FakeAuthRepository : public AuthRepository {
    public:
        struct User {
            LoginUser login;
            std::string email;
            int failedAttempts = 0;
            bool isAdmin = false;
        };
        std::map<int, User> users;
        std::map<int, StoredYubikey> keys;                 // by user ID
        std::map<std::string, SessionRecord> sessions;    // by ID hash
        std::vector<std::string> audits;
        std::vector<std::string> incidents;

        std::optional<LoginUser> findUserByUsername(const std::string& username) override
        {
            for (const auto& [id, user] : users) {
                if (user.login.username == username) return user.login;
            }
            return std::nullopt;
        }
        std::optional<LoginUser> findUserByEmail(const std::string& email) override
        {
            for (const auto& [id, user] : users) {
                if (user.email == email) return user.login;
            }
            return std::nullopt;
        }
        std::optional<LoginUser> findUserByID(int userID) override
        {
            const auto found = users.find(userID);
            return found == users.end() ? std::nullopt : std::optional(found->second.login);
        }
        bool recordFailedAttempt(int userID, int maxAttempts, std::int64_t lockUntil) override
        {
            User& user = users.at(userID);
            if (user.failedAttempts + 1 >= maxAttempts) {
                user.login.lockedUntil = lockUntil;
                user.failedAttempts = 0;
                return true;
            }
            ++user.failedAttempts;
            return false;
        }
        void resetFailedAttempts(int userID) override
        {
            users.at(userID).failedAttempts = 0;
            users.at(userID).login.lockedUntil.reset();
        }
        std::optional<StoredYubikey> findYubikey(int userID) override
        {
            const auto found = keys.find(userID);
            return found == keys.end() ? std::nullopt : std::optional(found->second);
        }
        bool advanceOtpCounters(int keyID, int counter, int sessionUse) override
        {
            for (auto& [userID, key] : keys) {
                if (key.keyID == keyID && (key.lastCounter < counter ||
                    (key.lastCounter == counter && key.lastSessionUse < sessionUse))) {
                    key.lastCounter = counter;
                    key.lastSessionUse = sessionUse;
                    return true;
                }
            }
            return false;
        }
        void createSession(const NewSession& s) override
        {
            const User& user = users.at(s.userID);
            sessions[s.idHash] = SessionRecord{s.idHash, s.userID, user.login.username, user.isAdmin,
                                               user.login.status == "active", s.mfaVerified,
                                               s.createdAt, s.createdAt, s.expiresAt};
        }
        std::optional<SessionRecord> findSession(const std::string& idHash) override
        {
            const auto found = sessions.find(idHash);
            return found == sessions.end() ? std::nullopt : std::optional(found->second);
        }
        void touchSession(const std::string& idHash, std::int64_t now) override
        {
            sessions.at(idHash).lastSeenAt = now;
        }
        void deleteSession(const std::string& idHash) override { sessions.erase(idHash); }
        void audit(std::string_view, std::string_view description, std::string_view,
                   std::optional<int>, std::string_view) override
        {
            audits.emplace_back(description);
        }
        void incident(std::string_view, std::string_view description, std::string_view,
                      std::optional<int>, std::string_view) override
        {
            incidents.emplace_back(description);
        }
};

class CountingHasher : public PasswordHasher {
    public:
        int verifications = 0;
        std::string hash(std::string_view password) override { return "h:" + std::string(password); }
        bool verify(std::string_view password, std::string_view stored) override
        {
            ++verifications;
            return stored == "h:" + std::string(password);
        }
};

// A user "pierrot" (ID 7) with password "Clave segura 2026" and the test
// YubiKey registered with counter 9, session use 2.
struct AuthFixture {
    std::int64_t clock = 1'800'000'000;
    FakeAuthRepository repository;
    CountingHasher hasher;
    std::array<unsigned char, 32> masterKey{};
    AesGcmSecretBox box;
    LibyubikeyOtpDecoder decoder;
    RandomSessionTokens tokens;
    Authentication auth;

    AuthFixture()
        : box(AesGcmSecretBox::fromKey(masterKey)),
          auth(repository, hasher, box, decoder, tokens, [this] { return clock; })
    {
        FakeAuthRepository::User user;
        user.login = LoginUser{7, "pierrot", "h:Clave segura 2026", "active", std::nullopt};
        user.email = "pierrot@nac.ucr.ac.cr";
        repository.users[7] = user;

        const std::vector<unsigned char> aesKey = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
                                                   0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
        const std::vector<unsigned char> uid = {0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};
        StoredYubikey key;
        key.keyID = 3;
        key.publicID = kTestPublicID;
        key.aesKeySealed = box.seal(aesKey, "yubikey-aes-key:" + kTestPublicID);
        key.privateIDSealed = box.seal(uid, "yubikey-private-id:" + kTestPublicID);
        key.lastCounter = 9;
        key.lastSessionUse = 2;
        repository.keys[7] = key;
    }

    std::string password() const { return "Clave segura 2026"; }
    std::string otp(std::uint16_t counter, std::uint8_t use = 0) const
    {
        return makeOtp(kTestAesKey, kTestPublicID, counter, use);
    }

    // Full login; returns the session token (empty on failure).
    std::string login(std::uint16_t counter)
    {
        const LoginOutcome first = auth.loginWithPassword("pierrot", password(), "10.0.0.5");
        if (!first.success) return "";
        const SecondFactorOutcome second = auth.verifySecondFactor(first.pendingToken, otp(counter), "10.0.0.5");
        return second.status == SecondFactorOutcome::Status::Success ? second.sessionToken : "";
    }
};

bool contains(const std::vector<std::string>& items, const std::string& text)
{
    for (const std::string& item : items) {
        if (item.find(text) != std::string::npos) return true;
    }
    return false;
}

void testPasswordStep()
{
    AuthFixture f;

    // Unknown user: fails, but a password check still runs (same timing).
    CHECK(!f.auth.loginWithPassword("nadie", "x", "10.0.0.5").success);
    CHECK(f.hasher.verifications == 1);
    CHECK(contains(f.repository.audits, "usuario inexistente"));
    CHECK(!f.auth.loginWithPassword("x' OR '1'='1", "x", "10.0.0.5").success);  // not even looked up

    // Correct password: a pending session, stored only as a hash.
    const LoginOutcome ok = f.auth.loginWithPassword("pierrot", f.password(), "10.0.0.5");
    CHECK(ok.success && Authentication::isWellFormedToken(ok.pendingToken));
    CHECK(f.repository.sessions.count(ok.pendingToken) == 0);
    const auto pending = f.repository.findSession(f.tokens.hash(ok.pendingToken));
    CHECK(pending && !pending->mfaVerified && pending->expiresAt == f.clock + 300);

    // Login by email, case-insensitive.
    CHECK(f.auth.loginWithPassword("Pierrot@NAC.ucr.ac.cr", f.password(), "10.0.0.5").success);

    // A pending session is not a full one.
    const auto info = f.auth.resolveSession(ok.pendingToken);
    CHECK(info && !info->fullyAuthenticated);

    // Inactive accounts cannot log in.
    f.repository.users[7].login.status = "pending";
    CHECK(!f.auth.loginWithPassword("pierrot", f.password(), "10.0.0.5").success);
}

void testLockout()
{
    AuthFixture f;
    CHECK(!f.auth.loginWithPassword("pierrot", "mala 1", "10.0.0.5").success);
    CHECK(!f.auth.loginWithPassword("pierrot", "mala 2", "10.0.0.5").success);
    CHECK(f.repository.incidents.empty());
    CHECK(!f.auth.loginWithPassword("pierrot", "mala 3", "10.0.0.5").success);  // locks
    CHECK(contains(f.repository.incidents, "bloqueada"));

    // Even the right password fails while locked...
    CHECK(!f.auth.loginWithPassword("pierrot", f.password(), "10.0.0.5").success);
    // ...and works again when the lock expires (temporary lock, row 20).
    f.clock += 15 * 60;
    CHECK(f.auth.loginWithPassword("pierrot", f.password(), "10.0.0.5").success);
}

void testSecondFactor()
{
    AuthFixture f;
    const LoginOutcome first = f.auth.loginWithPassword("pierrot", f.password(), "10.0.0.5");
    const SecondFactorOutcome second = f.auth.verifySecondFactor(first.pendingToken, f.otp(10), "10.0.0.5");
    CHECK(second.status == SecondFactorOutcome::Status::Success);

    // A NEW identifier; the pending one is gone (fixation, row 3).
    CHECK(second.sessionToken != first.pendingToken);
    CHECK(!f.auth.resolveSession(first.pendingToken));
    const auto info = f.auth.resolveSession(second.sessionToken);
    CHECK(info && info->fullyAuthenticated && info->username == "pierrot");
    CHECK(f.repository.keys[7].lastCounter == 10);

    // Replay: the same OTP in a new login is refused, with an incident (row 21).
    const LoginOutcome again = f.auth.loginWithPassword("pierrot", f.password(), "10.0.0.5");
    CHECK(f.auth.verifySecondFactor(again.pendingToken, f.otp(10), "10.0.0.5").status ==
          SecondFactorOutcome::Status::Rejected);
    CHECK(contains(f.repository.incidents, "reutilizar"));
    // An older OTP too (second failure)...
    CHECK(f.auth.verifySecondFactor(again.pendingToken, f.otp(9, 7), "10.0.0.5").status ==
          SecondFactorOutcome::Status::Rejected);
    // ...but a newer one works: same usage counter, higher session counter.
    CHECK(f.auth.verifySecondFactor(again.pendingToken, f.otp(10, 3), "10.0.0.5").status ==
          SecondFactorOutcome::Status::Success);
    CHECK(f.repository.keys[7].lastCounter == 10 && f.repository.keys[7].lastSessionUse == 3);
}

void testSecondFactorRejections()
{
    AuthFixture f;
    LoginOutcome first = f.auth.loginWithPassword("pierrot", f.password(), "10.0.0.5");

    // An OTP from another YubiKey (other public ID, other key).
    CHECK(f.auth.verifySecondFactor(first.pendingToken,
              makeOtp("ffeeddccbbaa99887766554433221100", "vvccccbcdefg", 50, 0), "10.0.0.5").status ==
          SecondFactorOutcome::Status::Rejected);
    // Malformed.
    CHECK(f.auth.verifySecondFactor(first.pendingToken, "abc", "10.0.0.5").status ==
          SecondFactorOutcome::Status::Rejected);
    // Third failure: locked, and the pending session is deleted.
    CHECK(f.auth.verifySecondFactor(first.pendingToken, "abc", "10.0.0.5").status ==
          SecondFactorOutcome::Status::Locked);
    CHECK(!f.auth.resolveSession(first.pendingToken));
    CHECK(!f.auth.loginWithPassword("pierrot", f.password(), "10.0.0.5").success);

    // The pending session expires after 5 minutes.
    AuthFixture g;
    first = g.auth.loginWithPassword("pierrot", g.password(), "10.0.0.5");
    g.clock += 300;
    CHECK(g.auth.verifySecondFactor(first.pendingToken, g.otp(10), "10.0.0.5").status ==
          SecondFactorOutcome::Status::SessionExpired);

    // Altered sealed secrets in the database: rejected, with an integrity incident.
    AuthFixture h;
    h.repository.keys[7].privateIDSealed[20] ^= 0x01;
    first = h.auth.loginWithPassword("pierrot", h.password(), "10.0.0.5");
    CHECK(h.auth.verifySecondFactor(first.pendingToken, h.otp(10), "10.0.0.5").status ==
          SecondFactorOutcome::Status::Rejected);
    CHECK(contains(h.repository.incidents, "secretos"));

    // A token that is not well formed is never looked up.
    CHECK(h.auth.verifySecondFactor("' OR 1=1 --", h.otp(11), "10.0.0.5").status ==
          SecondFactorOutcome::Status::SessionExpired);
}

void testSessionLifetime()
{
    AuthFixture f;
    const std::string token = f.login(10);
    CHECK(!token.empty());

    // Activity keeps it alive; 15 minutes of inactivity end it.
    f.clock += 14 * 60;
    CHECK(f.auth.resolveSession(token));
    f.clock += 14 * 60;
    CHECK(f.auth.resolveSession(token));
    f.clock += 15 * 60;
    CHECK(!f.auth.resolveSession(token));
    CHECK(f.repository.sessions.empty());  // deleted on the server

    // Absolute limit: 8 hours, even with constant activity.
    const std::string second = f.login(11);
    for (int i = 0; i < 47; ++i) {  // 47 * 10 minutes = 7 h 50 min
        f.clock += 10 * 60;
        CHECK(f.auth.resolveSession(second));
    }
    f.clock += 10 * 60;
    CHECK(!f.auth.resolveSession(second));

    // Logout deletes the session on the server.
    const std::string third = f.login(12);
    f.auth.logout(third, "10.0.0.5");
    CHECK(!f.auth.resolveSession(third));
    CHECK(contains(f.repository.audits, "Cierre de sesion"));
}

bool setsCookie(const CgiResponse& response, const std::string& fragment)
{
    for (const auto& [name, value] : response.headers) {
        if (name == "Set-Cookie" && value.find(fragment) != std::string::npos) return true;
    }
    return false;
}

std::string location(const CgiResponse& response)
{
    for (const auto& [name, value] : response.headers) {
        if (name == "Location") return value;
    }
    return "";
}

void testAccessControlRoutes()
{
    const std::string fullToken(64, 'a');
    const std::string pendingToken(64, 'b');
    UseCases useCases;
    useCases.listCatalog = [](int) { return CatalogPage{}; };
    int resolved = 0;
    useCases.resolveSession = [&](const std::string& token) -> std::optional<SessionInfo> {
        ++resolved;
        if (token == fullToken) return SessionInfo{7, "pierrot", false, true};
        if (token == pendingToken) return SessionInfo{7, "pierrot", false, false};
        return std::nullopt;
    };
    const std::string cookieName = kSessionCookieName;
    auto get = [&](const std::string& path, const std::string& cookie) {
        return route(CgiRequest("GET", path, "/app.cgi", "", "", "", "10.0.0.5", false,
                                cookie.empty() ? "" : cookieName + "=" + cookie),
                     useCases);
    };

    // Protected page: only with a full session.
    CHECK(get("/cuenta", "").status == 303 && location(get("/cuenta", "")) == "/app.cgi/login");
    CHECK(get("/cuenta", pendingToken).status == 303);
    const CgiResponse account = get("/cuenta", fullToken);
    CHECK(account.status == 200 && account.body.find("pierrot") != std::string::npos);
    CHECK(account.body.find("Cerrar sesión") != std::string::npos);

    // The YubiKey step: only with a pending session.
    CHECK(get("/login/yubikey", "").status == 303);
    CHECK(get("/login/yubikey", fullToken).status == 303);
    CHECK(get("/login/yubikey", pendingToken).status == 200);

    // A logged-in user asking for the login page goes to their account.
    CHECK(location(get("/login", fullToken)) == "/app.cgi/cuenta");

    // A stale cookie is deleted; a malformed one is not even looked up.
    CHECK(setsCookie(get("/catalogo", std::string(64, 'c')), "Max-Age=0"));
    resolved = 0;
    get("/catalogo", "zz' OR 1=1");
    CHECK(resolved == 0);
    CHECK(!setsCookie(get("/catalogo", fullToken), "Set-Cookie"));

    // Only fixed notices; the parameter is never printed.
    const CgiResponse notice = route(CgiRequest("GET", "/login", "/app.cgi", "aviso=%3Cscript%3E"), useCases);
    CHECK(notice.body.find("<script>") == std::string::npos && notice.body.find("&lt;script") == std::string::npos);
}

void testLoginRoutes()
{
    UseCases useCases;
    std::string loggedOut;
    SecondFactorOutcome::Status nextStatus = SecondFactorOutcome::Status::Success;
    useCases.resolveSession = [](const std::string& token) -> std::optional<SessionInfo> {
        if (token == std::string(64, 'b')) return SessionInfo{7, "pierrot", false, false};
        return std::nullopt;
    };
    useCases.loginWithPassword = [](std::string login, std::string password, const std::string&) {
        LoginOutcome outcome;
        if (login == "pierrot" && password == "Clave segura 2026") {
            outcome.success = true;
            outcome.pendingToken = std::string(64, 'b');
        }
        return outcome;
    };
    useCases.verifySecondFactor = [&](const std::string&, std::string, const std::string&) {
        return SecondFactorOutcome{nextStatus, std::string(64, 'd')};
    };
    useCases.logout = [&](const std::string& token, const std::string&) { loggedOut = token; };
    const std::string form = "application/x-www-form-urlencoded";
    const std::string pendingCookie = std::string(kSessionCookieName) + "=" + std::string(64, 'b');

    // Password step.
    const CgiResponse ok = route(CgiRequest("POST", "/login", "/app.cgi", "",
                                            "login=pierrot&password=Clave+segura+2026", form), useCases);
    CHECK(ok.status == 303 && location(ok) == "/app.cgi/login/yubikey");
    CHECK(setsCookie(ok, "__Host-jc_session=" + std::string(64, 'b')));
    CHECK(setsCookie(ok, "; Path=/; Secure; HttpOnly; SameSite=Lax"));
    CHECK(!setsCookie(ok, "Max-Age"));

    const CgiResponse bad = route(CgiRequest("POST", "/login", "/app.cgi", "",
                                             "login=%3Cb%3Epierrot&password=Secreta+mala", form), useCases);
    CHECK(bad.status == 422 && bad.body.find("No fue posible iniciar sesión") != std::string::npos);
    CHECK(bad.body.find("&lt;b&gt;pierrot") != std::string::npos);
    CHECK(bad.body.find("Secreta") == std::string::npos);
    CHECK(!setsCookie(bad, "jc_session="));

    // YubiKey step, each outcome.
    auto otpStep = [&] {
        return route(CgiRequest("POST", "/login/yubikey", "/app.cgi", "", "otp=x", form, "10.0.0.5",
                                false, pendingCookie), useCases);
    };
    CgiResponse step = otpStep();
    CHECK(step.status == 303 && location(step) == "/app.cgi/cuenta" &&
          setsCookie(step, "__Host-jc_session=" + std::string(64, 'd')));
    nextStatus = SecondFactorOutcome::Status::Rejected;
    CHECK(otpStep().status == 422);
    nextStatus = SecondFactorOutcome::Status::Locked;
    step = otpStep();
    CHECK(location(step) == "/app.cgi/login?aviso=bloqueo" && setsCookie(step, "Max-Age=0"));
    nextStatus = SecondFactorOutcome::Status::SessionExpired;
    CHECK(location(otpStep()) == "/app.cgi/login?aviso=expirada");

    // Logout: POST only, deletes the session and the cookie.
    CHECK(route(CgiRequest("GET", "/logout", "/app.cgi", ""), useCases).status == 405);
    const CgiResponse out = route(CgiRequest("POST", "/logout", "/app.cgi", "", "", form, "10.0.0.5",
                                             false, pendingCookie), useCases);
    CHECK(out.status == 303 && loggedOut == std::string(64, 'b') && setsCookie(out, "Max-Age=0"));
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

    useCases.getObjectHistory = [](int objectID) -> std::optional<ObjectHistory> {
        if (objectID != 5) {
            return std::nullopt;
        }
        ObjectHistory history;
        history.objectID = 5;
        history.objectName = "Atlas";
        return history;
    };
    CHECK(route(CgiRequest("GET", "/historial", "/app.cgi", "id=5"), useCases).status == 200);
    CHECK(route(CgiRequest("GET", "/historial", "/app.cgi", "id=6"), useCases).status == 404);
    CHECK(route(CgiRequest("GET", "/historial", "/app.cgi", "id=x"), useCases).status == 400);
    CHECK(route(CgiRequest("GET", "/historial", "/app.cgi", ""), useCases).status == 400);
    CHECK(route(CgiRequest("POST", "/historial", "/app.cgi", "id=5"), useCases).status == 405);
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
    testGetObjectHistory();
    testHistoryPage();
    testAccountRules();
    testScrypt();
    testSecretBox();
    testOtpDecoder();
    testRegisterUser();
    testFormRequests();
    testRegistrationRoutes();
    testPasswordStep();
    testLockout();
    testSecondFactor();
    testSecondFactorRejections();
    testSessionLifetime();
    testAccessControlRoutes();
    testLoginRoutes();
    testTemplateEngine();
    testEmbeddedTemplatesRender();
    testRouter();

    if (failures == 0) {
        std::cout << "All tests passed\n";
    }
    return failures == 0 ? 0 : 1;
}
