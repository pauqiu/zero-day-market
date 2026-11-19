#ifndef ACCOUNT_REPOSITORY_H
#define ACCOUNT_REPOSITORY_H

#include <cstdint>
#include <optional>
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
    // Email verification (HU-07): only the SHA-256 of the token is stored.
    std::string verificationTokenHash;
    std::int64_t verificationExpiresAt = 0;
};

struct ActivationResult {
    bool activated = false;
    int userID = 0;
    std::string username;
    int objectsAssigned = 0;
};

struct PendingAccount {
    int userID = 0;
    std::string username;
    std::string email;
    // Expiration of the newest unused verification token, if any.
    std::optional<std::int64_t> latestTokenExpiresAt;
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

        // Creates the user (pending until the email is verified), its YubiKey,
        // the verification token and the audit record in one transaction:
        // all of them or none (control: row 10).
        virtual CreateAccountResult createAccount(const NewAccount& account) = 0;

        // In one transaction (HU-07 and HU-08): checks that the token exists,
        // is unused and unexpired and that its user is pending; marks the
        // token used; activates the user; assigns up to `objects` objects of
        // the initial inventory, recording each transfer. The objects are
        // locked while assigned, so two activations never get the same one.
        virtual ActivationResult activateAccount(const std::string& tokenHash, std::int64_t now,
                                                 int objects, const std::string& clientIP) = 0;

        virtual std::optional<PendingAccount> findPendingAccount(const std::string& email) = 0;

        // Invalidates the unused tokens of the user and stores a new one (RF-2.9).
        virtual void replaceVerificationToken(int userID, const std::string& tokenHash,
                                              std::int64_t expiresAt, std::int64_t now) = 0;

        // Audit log entry outside any transaction.
        virtual void recordEvent(std::string_view category, std::string_view description,
                                 std::string_view clientIP, std::optional<int> userID,
                                 std::string_view details) = 0;

        // Security incident outside any transaction (enunciado, section 5.1).
        virtual void recordIncident(std::string_view category, std::string_view description,
                                    std::string_view clientIP, std::string_view details) = 0;
};

#endif // ACCOUNT_REPOSITORY_H
