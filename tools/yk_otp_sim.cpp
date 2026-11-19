// Development tool: generates a Yubico OTP from a known AES key, as a
// YubiKey would. Used to test registration and login without a physical key.
//
//   yk-otp-sim <aes key hex 32> <public id modhex 12> <private id hex 12> <counter> <session use>
//   yk-otp-sim 00112233445566778899aabbccddeeff cccccbcdefgh 0a0b0c0d0e0f 5 0
//
// It also demonstrates a limit of validating Yubico OTP locally: whoever
// knows the AES key can produce valid OTPs without the hardware. Never
// deployed (it is not part of dist/).

#include <yubikey.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {

bool hasLengthAndAlphabet(const char* text, std::size_t length, const char* alphabet)
{
    if (std::strlen(text) != length) {
        return false;
    }
    for (const char* c = text; *c; ++c) {
        if (!std::strchr(alphabet, *c)) {
            return false;
        }
    }
    return true;
}

}  // namespace

int main(int argc, char** argv)
{
    if (argc != 6 || !hasLengthAndAlphabet(argv[1], 32, "0123456789abcdef") ||
        !hasLengthAndAlphabet(argv[2], 12, YUBIKEY_MODHEX_MAP) ||
        !hasLengthAndAlphabet(argv[3], 12, "0123456789abcdef")) {
        std::fprintf(stderr, "usage: %s <aes key: 32 hex> <public id: 12 modhex> "
                             "<private id: 12 hex> <counter 0-32767> <session use 0-255>\n",
                     argv[0]);
        return 2;
    }
    const long counter = std::strtol(argv[4], nullptr, 10);
    const long sessionUse = std::strtol(argv[5], nullptr, 10);
    if (counter < 0 || counter > 0x7fff || sessionUse < 0 || sessionUse > 0xff) {
        std::fprintf(stderr, "counter must be 0-32767 and session use 0-255\n");
        return 2;
    }

    char key[YUBIKEY_KEY_SIZE + 1];
    char uid[YUBIKEY_UID_SIZE + 1];
    // The last argument is the size of the destination buffer (bytes + zero).
    yubikey_hex_decode(key, argv[1], sizeof key);
    yubikey_hex_decode(uid, argv[3], sizeof uid);

    yubikey_token_st token{};
    std::memcpy(token.uid, uid, YUBIKEY_UID_SIZE);
    token.ctr = static_cast<std::uint16_t>(counter);
    token.use = static_cast<std::uint8_t>(sessionUse);
    token.tstpl = 0x1234;
    token.tstph = 0x56;
    token.rnd = static_cast<std::uint16_t>(std::rand());

    // The YubiKey stores the complement of the CRC-16 of the first 14 bytes.
    // yubikey_generate does not compute it (it leaves it as is), so we do.
    token.crc = static_cast<std::uint16_t>(
        ~yubikey_crc16(reinterpret_cast<const std::uint8_t*>(&token), YUBIKEY_BLOCK_SIZE - 2));

    // yubikey_generate encrypts and modhex-encodes the block; it writes 32
    // characters plus a terminating zero.
    char block[2 * YUBIKEY_BLOCK_SIZE + 1];
    yubikey_generate(&token, reinterpret_cast<const std::uint8_t*>(key), block);

    std::printf("%s%s\n", argv[2], block);
    return 0;
}
