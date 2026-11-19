#ifndef HEX_H
#define HEX_H

#include <optional>
#include <string>
#include <string_view>
#include <vector>

std::string hexEncode(const unsigned char* data, std::size_t size);

// Empty if the text has an odd length or a non-hexadecimal character.
std::optional<std::vector<unsigned char>> hexDecode(std::string_view text);

#endif // HEX_H
