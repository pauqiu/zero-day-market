#include "infrastructure/mail/smtp_transport.h"

#include <curl/curl.h>

#include <cstring>
#include <memory>

#include "infrastructure/mail/mail_error.h"

namespace {

struct Upload {
    const std::string* data;
    std::size_t offset = 0;
};

// libcurl asks for the message in pieces.
std::size_t readMessage(char* buffer, std::size_t size, std::size_t count, void* userData)
{
    Upload* upload = static_cast<Upload*>(userData);
    const std::size_t room = size * count;
    const std::size_t remaining = upload->data->size() - upload->offset;
    const std::size_t chunk = remaining < room ? remaining : room;
    std::memcpy(buffer, upload->data->data() + upload->offset, chunk);
    upload->offset += chunk;
    return chunk;
}

using Easy = std::unique_ptr<CURL, decltype(&curl_easy_cleanup)>;
using List = std::unique_ptr<curl_slist, decltype(&curl_slist_free_all)>;

}  // namespace

SmtpTransport::SmtpTransport(MailConfig config) : config(std::move(config))
{
}

void SmtpTransport::send(const std::string& to, const std::string& message)
{
    Easy curl(curl_easy_init(), &curl_easy_cleanup);
    if (!curl) {
        throw MailError("curl_easy_init failed");
    }
    CURL* c = curl.get();
    List recipients(curl_slist_append(nullptr, to.c_str()), &curl_slist_free_all);
    Upload upload{&message};

    curl_easy_setopt(c, CURLOPT_URL, config.url.c_str());
    // Only SMTP: the URL cannot be turned into any other protocol.
#if LIBCURL_VERSION_NUM >= 0x075500  /* 7.85.0 */
    curl_easy_setopt(c, CURLOPT_PROTOCOLS_STR, "smtp,smtps");
#else
    curl_easy_setopt(c, CURLOPT_PROTOCOLS, static_cast<long>(CURLPROTO_SMTP | CURLPROTO_SMTPS));
#endif

    // TLS is mandatory: with smtp:// the connection must upgrade with
    // STARTTLS or it is closed before authenticating (credentials never
    // travel in clear text).
    curl_easy_setopt(c, CURLOPT_USE_SSL, static_cast<long>(CURLUSESSL_ALL));

    // Server identity (control: row 22). The course server uses a
    // self-signed certificate. Verification is NEVER simply disabled:
    //   * pinned key: the connection is accepted only if the server presents
    //     exactly this public key, whatever its certificate says;
    //   * CA file: the certificate must be signed by it and match the name.
    if (!config.caFile.empty()) {
        curl_easy_setopt(c, CURLOPT_CAINFO, config.caFile.c_str());
        curl_easy_setopt(c, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(c, CURLOPT_SSL_VERIFYHOST, 2L);
    } else {
        // A self-signed certificate cannot pass the CA check; the pin
        // replaces it and libcurl still enforces it with VERIFYPEER off.
        curl_easy_setopt(c, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(c, CURLOPT_SSL_VERIFYHOST, 0L);
    }
    if (!config.pinnedPublicKey.empty()) {
        curl_easy_setopt(c, CURLOPT_PINNEDPUBLICKEY, config.pinnedPublicKey.c_str());
    }

    if (!config.username.empty()) {
        curl_easy_setopt(c, CURLOPT_USERNAME, config.username.c_str());
        curl_easy_setopt(c, CURLOPT_PASSWORD, config.password.c_str());
    }
    curl_easy_setopt(c, CURLOPT_MAIL_FROM, config.from.c_str());
    curl_easy_setopt(c, CURLOPT_MAIL_RCPT, recipients.get());
    curl_easy_setopt(c, CURLOPT_READFUNCTION, readMessage);
    curl_easy_setopt(c, CURLOPT_READDATA, &upload);
    curl_easy_setopt(c, CURLOPT_UPLOAD, 1L);

    // A slow or unreachable server must not hang the request.
    curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT, 5L);
    curl_easy_setopt(c, CURLOPT_TIMEOUT, 15L);
    curl_easy_setopt(c, CURLOPT_NOSIGNAL, 1L);

    const CURLcode result = curl_easy_perform(c);
    if (result != CURLE_OK) {
        // The libcurl message never contains the password.
        throw MailError(std::string("SMTP delivery failed: ") + curl_easy_strerror(result));
    }
}
