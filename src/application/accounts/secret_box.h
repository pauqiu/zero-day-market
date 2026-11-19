#ifndef SECRET_BOX_H
#define SECRET_BOX_H

#include <optional>
#include <string_view>
#include <vector>

// Port: authenticated encryption of secrets stored in the database, with a
// master key that lives outside it (control: row 14). `context` is bound to
// the ciphertext: a value sealed for one context cannot be opened as another,
// so ciphertexts cannot be swapped between rows or columns.
class SecretBox {
    public:
        virtual ~SecretBox() = default;

        virtual std::vector<unsigned char> seal(const std::vector<unsigned char>& plaintext,
                                                std::string_view context) = 0;

        // Empty if the value was altered, sealed for another context or with another key.
        virtual std::optional<std::vector<unsigned char>> open(
            const std::vector<unsigned char>& sealed, std::string_view context) = 0;
};

#endif // SECRET_BOX_H
