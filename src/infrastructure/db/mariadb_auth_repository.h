#ifndef MARIADB_AUTH_REPOSITORY_H
#define MARIADB_AUTH_REPOSITORY_H

#include "application/auth/auth_repository.h"
#include "infrastructure/db/mariadb_connection.h"

class MariaDbAuthRepository : public AuthRepository {
    public:
        explicit MariaDbAuthRepository(MariaDbConnection& connection);

        std::optional<LoginUser> findUserByUsername(const std::string& username) override;
        std::optional<LoginUser> findUserByEmail(const std::string& email) override;
        std::optional<LoginUser> findUserByID(int userID) override;
        bool recordFailedAttempt(int userID, int maxAttempts, std::int64_t lockUntil) override;
        void resetFailedAttempts(int userID) override;
        std::optional<StoredYubikey> findYubikey(int userID) override;
        bool advanceOtpCounters(int keyID, int counter, int sessionUse) override;
        void createSession(const NewSession& session) override;
        std::optional<SessionRecord> findSession(const std::string& idHash) override;
        void touchSession(const std::string& idHash, std::int64_t now) override;
        void deleteSession(const std::string& idHash) override;
        void audit(std::string_view category, std::string_view description,
                   std::string_view clientIP, std::optional<int> userID,
                   std::string_view details) override;
        void incident(std::string_view category, std::string_view description,
                      std::string_view clientIP, std::optional<int> userID,
                      std::string_view details) override;

    private:
        std::optional<LoginUser> findUser(const char* sql, MariaDbParams& params);
        void execute(const char* sql, MariaDbParams& params, const char* what);

        MariaDbConnection& connection;
};

#endif // MARIADB_AUTH_REPOSITORY_H
