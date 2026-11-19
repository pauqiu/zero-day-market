#ifndef ACCOUNT_REPOSITORY_H
#define ACCOUNT_REPOSITORY_H

#include <string>
#include <string_view>
#include <vector>

// Everything needed to create an account. Secrets arrive already protected:
// the password as a hash, the YubiKey AES key and private ID sealed.
struct NewAccount {
    std::string username;
    std::string email;
    std::string passwordHash;
    std::string publicID;
    std::vector<unsigned char> aesKeySealed;
    std::vector<unsigned char> privateIDSealed;
    int counter = 0;
    int sessionUse = 0;
    std::string clientIP;
};

enum class CreateAccountOutcome { Created, UsernameTaken, EmailTaken, YubikeyTaken };

struct CreateAccountResult {
    CreateAccountOutcome outcome = CreateAccountOutcome::Created;
    int userID = 0;
};

// Port: account storage.
class AccountRepository {
    public:
        virtual ~AccountRepository() = default;

        // Creates the user, its YubiKey and the audit record in one transaction:
        // all of them or none (control: row 10).
        virtual CreateAccountResult createAccount(const NewAccount& account) = 0;

        // Security incident outside any transaction (enunciado, section 5.1).
        virtual void recordIncident(std::string_view category, std::string_view description,
                                    std::string_view clientIP, std::string_view details) = 0;
};

#endif // ACCOUNT_REPOSITORY_H
