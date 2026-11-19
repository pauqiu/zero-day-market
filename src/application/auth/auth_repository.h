#ifndef AUTH_REPOSITORY_H
#define AUTH_REPOSITORY_H

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Times are UTC epoch seconds.

struct LoginUser {
    int userID = 0;
    std::string username;
    std::string email;
    std::string passwordHash;
    std::string status;                       // pending, verified, active
    std::optional<std::int64_t> lockedUntil;  // temporary lockout (row 20)
};

struct StoredYubikey {
    int keyID = 0;
    std::string publicID;
    std::vector<unsigned char> aesKeySealed;
    std::vector<unsigned char> privateIDSealed;
    int lastCounter = 0;
    int lastSessionUse = 0;
};

struct NewSession {
    std::string idHash;
    int userID = 0;
    std::string clientIP;
    bool mfaVerified = false;
    std::int64_t createdAt = 0;
    std::int64_t expiresAt = 0;
};

struct SessionRecord {
    std::string idHash;
    int userID = 0;
    std::string username;
    bool isAdmin = false;
    bool userActive = false;
    bool mfaVerified = false;
    std::int64_t createdAt = 0;
    std::int64_t lastSeenAt = 0;
    std::int64_t expiresAt = 0;
};

// Port: what the authentication flow needs from storage.
class AuthRepository {
    public:
        virtual ~AuthRepository() = default;

        virtual std::optional<LoginUser> findUserByUsername(const std::string& username) = 0;
        virtual std::optional<LoginUser> findUserByEmail(const std::string& email) = 0;
        virtual std::optional<LoginUser> findUserByID(int userID) = 0;

        // Adds one failed attempt. When it reaches maxAttempts, the account is
        // locked until lockUntil and the counter starts over. Returns true if
        // this attempt locked the account. Atomic in the database.
        virtual bool recordFailedAttempt(int userID, int maxAttempts, std::int64_t lockUntil) = 0;
        virtual void resetFailedAttempts(int userID) = 0;

        virtual std::optional<StoredYubikey> findYubikey(int userID) = 0;

        // Stores the counters of an accepted OTP only if they are newer than
        // the stored ones, in a single statement. Returns false if another
        // request already used this OTP or a newer one (control: row 21).
        virtual bool advanceOtpCounters(int keyID, int counter, int sessionUse) = 0;

        virtual void createSession(const NewSession& session) = 0;
        virtual std::optional<SessionRecord> findSession(const std::string& idHash) = 0;
        virtual void touchSession(const std::string& idHash, std::int64_t now) = 0;
        virtual void deleteSession(const std::string& idHash) = 0;

        // Audit log and security incidents (control: row 10).
        virtual void audit(std::string_view category, std::string_view description,
                           std::string_view clientIP, std::optional<int> userID,
                           std::string_view details) = 0;
        virtual void incident(std::string_view category, std::string_view description,
                              std::string_view clientIP, std::optional<int> userID,
                              std::string_view details) = 0;
};

#endif // AUTH_REPOSITORY_H
