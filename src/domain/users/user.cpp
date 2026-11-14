#include "user.h"

User::User(int userID, 
            std::string username, 
            std::string email,
            std::string password): 
            userID(userID), 
            username(username),
            email(email),
            password(password)
{
}

User::~User()
{
}
