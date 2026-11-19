#ifndef REGISTER_PAGE_H
#define REGISTER_PAGE_H

#include <string>
#include <vector>

#include "application/accounts/register_user.h"
#include "presentation/templates/template_engine.h"

// Registration form (templates/register.html). Only the username and email
// are written back; the secrets never are.
Html renderRegisterBody(const std::string& scriptName, const std::string& username,
                        const std::string& email, const std::vector<RegistrationError>& errors);

Html renderRegisterDoneBody(const std::string& scriptName, const std::string& username);

// Message shown to the user for each error.
std::string registrationErrorMessage(RegistrationError error);

#endif // REGISTER_PAGE_H
