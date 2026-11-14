#ifndef USER_H
#define USER_H

#include <fstream>
#include <string>

enum Status {
    VERIFIED,
    PENDING
};

// TODO: Add 2F Authentication attributes

class User {
    private:
        int userID;
        std::string username;
        std::string email;
        int failedAttempts = 0;
        std::string password;

    public:
        User(int userID,
            std::string username,
            std::string email,
            std::string password
        );

        ~User();
};

#endif USER_H // USER_H