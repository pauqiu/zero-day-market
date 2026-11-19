#ifndef MARIADB_ACCOUNT_REPOSITORY_H
#define MARIADB_ACCOUNT_REPOSITORY_H

#include "application/accounts/account_repository.h"
#include "infrastructure/db/mariadb_connection.h"

class MariaDbAccountRepository : public AccountRepository {
    public:
        explicit MariaDbAccountRepository(MariaDbConnection& connection);

        CreateAccountResult createAccount(const NewAccount& account) override;
        ActivationResult activateAccount(const std::string& tokenHash, std::int64_t now,
                                         int objects, const std::string& clientIP) override;
        std::optional<PendingAccount> findPendingAccount(const std::string& email) override;
        void replaceVerificationToken(int userID, const std::string& tokenHash,
                                      std::int64_t expiresAt, std::int64_t now) override;
        void recordEvent(std::string_view category, std::string_view description,
                         std::string_view clientIP, std::optional<int> userID,
                         std::string_view details) override;
        void recordIncident(std::string_view category, std::string_view description,
                            std::string_view clientIP, std::string_view details) override;

    private:
        MariaDbConnection& connection;
};

#endif // MARIADB_ACCOUNT_REPOSITORY_H
