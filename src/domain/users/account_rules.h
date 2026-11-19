#ifndef ACCOUNT_RULES_H
#define ACCOUNT_RULES_H

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

// Rules for new accounts (HU-06, RF-2.1 and RF-2.3).

constexpr std::size_t kUsernameMinLength = 3;
constexpr std::size_t kUsernameMaxLength = 32;
constexpr std::size_t kPasswordMinLength = 12;   // same policy as the base image (RI3)
constexpr std::size_t kPasswordMaxLength = 128;  // bounds the cost of hashing
constexpr std::size_t kEmailMaxLength = 254;

// 3 to 32 characters among letters, digits, '-' and '_'. Usernames never
// contain spaces, line breaks or symbols, so they are safe anywhere they
// appear (pages, logs, mail bodies).
bool isValidUsername(std::string_view username);

// Lowercase, validated email address, or nothing if it is not acceptable.
// Deliberately stricter than the RFC: no quotes, spaces, line breaks or
// comments, which also keeps addresses safe inside mail headers.
std::optional<std::string> normalizeEmail(std::string_view email);

enum class PasswordProblem { None, TooShort, TooLong, TooFewCharacterTypes };

// At least 12 characters and 3 of 4 types: lowercase, uppercase, digits, others.
PasswordProblem checkPasswordPolicy(std::string_view password);

#endif // ACCOUNT_RULES_H
