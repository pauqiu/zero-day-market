#ifndef MARIADB_ACCOUNT_REPOSITORY_H
#define MARIADB_ACCOUNT_REPOSITORY_H

#include "application/accounts/account_repository.h"
#include "infrastructure/db/mariadb_connection.h"

class MariaDbAccountRepository : public AccountRepository {
    public:
        explicit MariaDbAccountRepository(MariaDbConnection& connection);

        CreateAccountResult createAccount(const NewAccount& account) override;
        void recordIncident(std::string_view category, std::string_view description,
                            std::string_view clientIP, std::string_view details) override;

    private:
        MariaDbConnection& connection;
};

#endif // MARIADB_ACCOUNT_REPOSITORY_H
