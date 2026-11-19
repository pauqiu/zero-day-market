#ifndef SESSION_COOKIE_H
#define SESSION_COOKIE_H

#include <string>

// The session cookie (control: row 3).
//   __Host-  the browser only accepts it with Secure, Path=/ and no Domain,
//            so a subdomain or an insecure page cannot plant one (fixation)
//   Secure   only sent over HTTPS
//   HttpOnly not readable by JavaScript, which limits what an XSS can steal
//   SameSite=Lax  not sent with cross-site POSTs (control: row 4)
// No Max-Age: the browser forgets it when it closes. The real expiration is
// enforced on the server, in the sessions table.
inline constexpr const char* kSessionCookieName = "__Host-jc_session";

inline std::string sessionCookie(const std::string& token)
{
    return std::string(kSessionCookieName) + "=" + token + "; Path=/; Secure; HttpOnly; SameSite=Lax";
}

// Tells the browser to delete the cookie (logout, expired session).
inline std::string expiredSessionCookie()
{
    return std::string(kSessionCookieName) + "=; Path=/; Secure; HttpOnly; SameSite=Lax; Max-Age=0";
}

#endif // SESSION_COOKIE_H
