#include "application/accounts/register_user.h"

#include <optional>

#include "application/common/wipe.h"
#include "domain/users/account_rules.h"

namespace {

constexpr std::size_t kAesKeyHexLength = 32;  // 16 bytes
constexpr std::size_t kOtpLength = 44;        // 12 public ID + 32 encrypted block

bool isHex(char c)
{
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
}

bool isModhex(char c)
{
    return std::string_view("cbdefghijklnrtuv").find(c) != std::string_view::npos;
}

// YubiKeys type modhex in lowercase, but with Caps Lock on they type it in
// uppercase; the AES key may be written in either case.
std::string lowercase(std::string text)
{
    for (char& c : text) {
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c - 'A' + 'a');
        }
    }
    return text;
}

bool allOf(const std::string& text, std::size_t length, bool (*accept)(char))
{
    if (text.size() != length) {
        return false;
    }
    for (const char c : text) {
        if (!accept(c)) {
            return false;
        }
    }
    return true;
}

}  // namespace

RegisterUser::RegisterUser(AccountRepository& accounts, PasswordHasher& hasher,
                           SecretBox& secrets, YubikeyOtpDecoder& decoder, SessionTokens& tokens,
                           Notifier& notifier, Clock now)
    : accounts(accounts),
      hasher(hasher),
      secrets(secrets),
      decoder(decoder),
      tokens(tokens),
      notifier(notifier),
      now(std::move(now))
{
}

RegistrationResult RegisterUser::execute(RegistrationRequest request)
{
    RegistrationResult result;

    // Wipes every secret of the request on any return path.
    struct Cleanup {
        RegistrationRequest& r;
        ~Cleanup()
        {
            wipe(r.password);
            wipe(r.passwordConfirmation);
            wipe(r.aesKey);
            wipe(r.otp);
        }
    } cleanup{request};

    // 1. Formats and rules: cheap checks first, all reported at once.
    if (!isValidUsername(request.username)) {
        result.errors.push_back(RegistrationError::InvalidUsername);
    }
    const std::optional<std::string> email = normalizeEmail(request.email);
    if (!email) {
        result.errors.push_back(RegistrationError::InvalidEmail);
    }
    switch (checkPasswordPolicy(request.password)) {
        case PasswordProblem::TooShort:
            result.errors.push_back(RegistrationError::PasswordTooShort);
            break;
        case PasswordProblem::TooLong:
            result.errors.push_back(RegistrationError::PasswordTooLong);
            break;
        case PasswordProblem::TooFewCharacterTypes:
            result.errors.push_back(RegistrationError::PasswordTooSimple);
            break;
        case PasswordProblem::None:
            break;
    }
    if (request.password != request.passwordConfirmation) {
        result.errors.push_back(RegistrationError::PasswordMismatch);
    }
    request.aesKey = lowercase(std::move(request.aesKey));
    request.otp = lowercase(std::move(request.otp));
    if (!allOf(request.aesKey, kAesKeyHexLength, isHex)) {
        result.errors.push_back(RegistrationError::InvalidAesKeyFormat);
    }
    if (!allOf(request.otp, kOtpLength, isModhex)) {
        result.errors.push_back(RegistrationError::InvalidOtpFormat);
    }
    if (!result.errors.empty()) {
        return result;
    }

    // 2. The YubiKey: the AES key must decrypt the OTP into a block with a
    //    valid CRC. Otherwise the key is rejected with a generic message.
    std::optional<DecodedOtp> otp = decoder.decode(request.otp, request.aesKey);
    if (!otp) {
        accounts.recordIncident("validacion", "OTP de YubiKey invalido en un registro",
                                request.clientIP, "public_id=" + request.otp.substr(0, 12));
        result.errors.push_back(RegistrationError::YubikeyRejected);
        return result;
    }

    // 3. Protect the secrets before they reach storage: the password as a
    //    slow salted hash; the AES key and private ID sealed with the master
    //    key, each bound to this public ID and to its column.
    NewAccount account;
    account.username = request.username;
    account.email = *email;
    account.passwordHash = hasher.hash(request.password);
    account.publicID = otp->publicID;
    account.aesKeySealed = secrets.seal(otp->aesKey, "yubikey-aes-key:" + otp->publicID);
    account.privateIDSealed = secrets.seal(otp->privateID, "yubikey-private-id:" + otp->publicID);
    // The counters of this OTP become the last accepted ones, so this same
    // OTP can never be used to log in (control: row 21).
    account.counter = otp->counter;
    account.sessionUse = otp->sessionUse;
    account.clientIP = request.clientIP;
    wipe(otp->aesKey);
    wipe(otp->privateID);

    // The verification link (HU-07): a random token sent only by email; the
    // database keeps its hash, so a copy of it cannot activate accounts.
    std::string verificationToken = tokens.newToken();
    account.verificationTokenHash = tokens.hash(verificationToken);
    account.verificationExpiresAt = now() + kVerificationSeconds;

    // 4. Store everything in one transaction. Unique constraints decide the
    //    races between simultaneous registrations.
    const CreateAccountResult created = accounts.createAccount(account);
    switch (created.outcome) {
        case CreateAccountOutcome::Created:
            result.success = true;
            result.userID = created.userID;
            result.username = account.username;
            result.email = account.email;
            // The account exists even if the email fails: the user can ask
            // for the link again (RF-2.9), and the failure is logged.
            result.verificationSent =
                notifier.sendVerification(account.email, account.username, verificationToken);
            if (!result.verificationSent) {
                accounts.recordEvent("correo", "No se pudo enviar el correo de verificacion",
                                     request.clientIP, created.userID, "");
            }
            break;
        case CreateAccountOutcome::UsernameTaken:
            result.errors.push_back(RegistrationError::UsernameTaken);
            break;
        case CreateAccountOutcome::EmailTaken:
            result.errors.push_back(RegistrationError::EmailTaken);
            break;
        case CreateAccountOutcome::YubikeyTaken:
            accounts.recordIncident("validacion", "Registro con una YubiKey ya registrada",
                                    request.clientIP, "public_id=" + account.publicID);
            result.errors.push_back(RegistrationError::YubikeyRejected);
            break;
    }
    wipe(verificationToken);
    return result;
}
