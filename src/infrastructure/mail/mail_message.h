#ifndef MAIL_MESSAGE_H
#define MAIL_MESSAGE_H

#include <cstdint>
#include <optional>
#include <string>

// Builds a plain-text email (RFC 5322 / MIME), or nothing if an address is
// not acceptable. This is the second barrier against header injection
// (control: row 23), after the validation at registration:
//   * From and To must be plain addresses: no spaces, quotes, angle brackets,
//     commas or line breaks, so they cannot add headers or recipients;
//   * the subject and the body are encoded in base64 (UTF-8), so no text of
//     the message can ever produce a line break inside the headers.
std::optional<std::string> buildMailMessage(const std::string& from, const std::string& to,
                                            const std::string& subject, const std::string& body,
                                            std::int64_t dateEpoch);

// True for a plain address such as ana.p@nac.ucr.ac.cr.
bool isPlainAddress(const std::string& address);

#endif // MAIL_MESSAGE_H
