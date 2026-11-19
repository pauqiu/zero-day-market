#ifndef CONSTANT_TIME_H
#define CONSTANT_TIME_H

#include <string>
#include <vector>

// Compares secrets in time that does not depend on where they differ, so the
// response time does not reveal how many leading bytes were right.
inline bool constantTimeEqual(const std::vector<unsigned char>& a, const std::vector<unsigned char>& b)
{
    if (a.size() != b.size()) {
        return false;
    }
    unsigned char difference = 0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        difference |= static_cast<unsigned char>(a[i] ^ b[i]);
    }
    return difference == 0;
}

inline std::string toHex(const std::vector<unsigned char>& bytes)
{
    static const char digits[] = "0123456789abcdef";
    std::string out;
    out.reserve(bytes.size() * 2);
    for (const unsigned char b : bytes) {
        out += digits[b >> 4];
        out += digits[b & 0x0f];
    }
    return out;
}

#endif // CONSTANT_TIME_H
