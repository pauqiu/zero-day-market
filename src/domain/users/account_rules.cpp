#include "domain/users/account_rules.h"

namespace {

bool isAsciiLetterOrDigit(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
}

char toLowerAscii(char c)
{
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

bool isValidLocalPart(std::string_view local)
{
    if (local.empty() || local.size() > 64 || local.front() == '.' || local.back() == '.') {
        return false;
    }
    char previous = '\0';
    for (const char c : local) {
        const bool ok = isAsciiLetterOrDigit(c) || c == '.' || c == '_' || c == '%' ||
                        c == '+' || c == '-';
        if (!ok || (c == '.' && previous == '.')) {
            return false;
        }
        previous = c;
    }
    return true;
}

bool isValidDomain(std::string_view domain)
{
    std::size_t labels = 0;
    std::size_t start = 0;
    while (start <= domain.size()) {
        std::size_t end = domain.find('.', start);
        if (end == std::string_view::npos) {
            end = domain.size();
        }
        const std::string_view label = domain.substr(start, end - start);
        if (label.empty() || label.size() > 63 || label.front() == '-' || label.back() == '-') {
            return false;
        }
        for (const char c : label) {
            if (!isAsciiLetterOrDigit(c) && c != '-') {
                return false;
            }
        }
        ++labels;
        start = end + 1;
    }
    return labels >= 2;
}

}  // namespace

bool isValidUsername(std::string_view username)
{
    if (username.size() < kUsernameMinLength || username.size() > kUsernameMaxLength) {
        return false;
    }
    for (const char c : username) {
        if (!isAsciiLetterOrDigit(c) && c != '-' && c != '_') {
            return false;
        }
    }
    return true;
}

std::optional<std::string> normalizeEmail(std::string_view email)
{
    if (email.empty() || email.size() > kEmailMaxLength) {
        return std::nullopt;
    }
    const std::size_t at = email.find('@');
    if (at == std::string_view::npos || email.find('@', at + 1) != std::string_view::npos) {
        return std::nullopt;
    }
    if (!isValidLocalPart(email.substr(0, at)) || !isValidDomain(email.substr(at + 1))) {
        return std::nullopt;
    }
    std::string normalized(email);
    for (char& c : normalized) {
        c = toLowerAscii(c);
    }
    return normalized;
}

PasswordProblem checkPasswordPolicy(std::string_view password)
{
    if (password.size() < kPasswordMinLength) {
        return PasswordProblem::TooShort;
    }
    if (password.size() > kPasswordMaxLength) {
        return PasswordProblem::TooLong;
    }
    bool lower = false, upper = false, digit = false, other = false;
    for (const char c : password) {
        if (c >= 'a' && c <= 'z') lower = true;
        else if (c >= 'A' && c <= 'Z') upper = true;
        else if (c >= '0' && c <= '9') digit = true;
        else other = true;  // symbols, spaces and any non-ASCII byte
    }
    const int types = lower + upper + digit + other;
    return types >= 3 ? PasswordProblem::None : PasswordProblem::TooFewCharacterTypes;
}
