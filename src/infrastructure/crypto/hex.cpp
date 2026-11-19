#include "infrastructure/crypto/hex.h"

namespace {

int hexValue(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

}  // namespace

std::string hexEncode(const unsigned char* data, std::size_t size)
{
    static const char digits[] = "0123456789abcdef";
    std::string out;
    out.reserve(size * 2);
    for (std::size_t i = 0; i < size; ++i) {
        out += digits[data[i] >> 4];
        out += digits[data[i] & 0x0f];
    }
    return out;
}

std::optional<std::vector<unsigned char>> hexDecode(std::string_view text)
{
    if (text.size() % 2 != 0) {
        return std::nullopt;
    }
    std::vector<unsigned char> out(text.size() / 2);
    for (std::size_t i = 0; i < out.size(); ++i) {
        const int high = hexValue(text[2 * i]);
        const int low = hexValue(text[2 * i + 1]);
        if (high < 0 || low < 0) {
            return std::nullopt;
        }
        out[i] = static_cast<unsigned char>(high * 16 + low);
    }
    return out;
}
