#include "application/auth/authentication.h"

#include "application/common/constant_time.h"
#include "application/common/wipe.h"
#include "domain/users/account_rules.h"

namespace {

// A valid scrypt hash, with the production parameters, of a password nobody
// uses. Checked when the user does not exist, so that an unknown username
// takes as long as a wrong password: the timing does not reveal which
// usernames exist.
constexpr const char* kDummyPasswordHash =
    "scrypt$15$8$1$00f978e8a5be79ea3947dfc90c52eec6$"
    "427f9be7cedc37c7de90afa15dc503c8c2094c1fd82d79fb2aa1e0b7cc611d3f";

constexpr std::size_t kMaxLoginLength = 254;
constexpr std::size_t kOtpLength = 44;

bool isModhexOtp(const std::string& otp)
{
    if (otp.size() != kOtpLength) {
        return false;
    }
    for (const char c : otp) {
        if (std::string_view("cbdefghijklnrtuv").find(c) == std::string_view::npos) {
            return false;
        }
    }
    return true;
}

// Login names written to the audit log: bounded, and only safe characters
// (the value comes from an unauthenticated visitor).
std::string loggable(const std::string& login)
{
    std::string out;
    for (const char c : login.substr(0, 64)) {
        const bool safe = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                          (c >= '0' && c <= '9') || c == '@' || c == '.' || c == '_' || c == '-';
        out += safe ? c : '?';
    }
    return out;
}

// The OTP is newer than the last accepted one: higher usage counter, or the
// same usage counter and a higher session counter.
bool isNewer(int counter, int sessionUse, int lastCounter, int lastSessionUse)
{
    return counter > lastCounter || (counter == lastCounter && sessionUse > lastSessionUse);
}

}  // namespace

Authentication::Authentication(AuthRepository& repository, PasswordHasher& hasher,
                               SecretBox& secrets, YubikeyOtpDecoder& decoder,
                               SessionTokens& tokens, Notifier& notifier, Clock now,
                               AuthPolicy policy)
    : repository(repository),
      hasher(hasher),
      secrets(secrets),
      decoder(decoder),
      tokens(tokens),
      notifier(notifier),
      now(std::move(now)),
      policy(policy)
{
}

void Authentication::notifyLock(const LoginUser& user, const std::string& clientIP)
{
    if (!notifier.sendLockNotice(user.email, user.username, now(), clientIP)) {
        repository.audit("correo", "No se pudo enviar el aviso de bloqueo", clientIP,
                         user.userID, "");
    }
}

void Authentication::notifyLogin(const LoginUser& user, const std::string& clientIP)
{
    if (!notifier.sendLoginNotice(user.email, user.username, now(), clientIP)) {
        repository.audit("correo", "No se pudo enviar el aviso de inicio de sesion", clientIP,
                         user.userID, "");
    }
}

bool Authentication::isWellFormedToken(const std::string& token)
{
    if (token.size() != 64) {
        return false;
    }
    for (const char c : token) {
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) {
            return false;
        }
    }
    return true;
}

LoginOutcome Authentication::loginWithPassword(std::string login, std::string password,
                                               const std::string& clientIP)
{
    struct Cleanup {
        std::string& password;
        ~Cleanup() { wipe(password); }
    } cleanup{password};

    const std::int64_t current = now();
    std::optional<LoginUser> user;
    if (!login.empty() && login.size() <= kMaxLoginLength) {
        if (login.find('@') != std::string::npos) {
            if (const auto email = normalizeEmail(login)) {
                user = repository.findUserByEmail(*email);
            }
        } else if (isValidUsername(login)) {
            user = repository.findUserByUsername(login);
        }
    }

    if (!user) {
        hasher.verify(password, kDummyPasswordHash);  // same cost as a real check
        repository.audit("autenticacion", "Inicio de sesion fallido: usuario inexistente",
                         clientIP, std::nullopt, "login=" + loggable(login));
        return {};
    }

    // The password is always checked, even for a locked account, so the
    // response time does not reveal the lock either.
    const bool passwordOk = hasher.verify(password, user->passwordHash);

    if (user->lockedUntil && *user->lockedUntil > current) {
        repository.audit("autenticacion", "Intento de inicio de sesion con cuenta bloqueada",
                         clientIP, user->userID, "");
        return {};
    }

    if (!passwordOk) {
        const bool lockedNow = repository.recordFailedAttempt(
            user->userID, policy.maxFailedAttempts, current + policy.lockSeconds);
        repository.audit("autenticacion", "Inicio de sesion fallido: contrasena incorrecta",
                         clientIP, user->userID, "");
        if (lockedNow) {
            repository.incident("autenticacion", "Cuenta bloqueada por intentos fallidos",
                                clientIP, user->userID, "factor=contrasena");
            notifyLock(*user, clientIP);
        }
        return {};
    }

    if (user->status != "active") {
        repository.audit("autenticacion", "Inicio de sesion con una cuenta no activa",
                         clientIP, user->userID, "status=" + user->status);
        return {};
    }

    // A session that only allows the YubiKey step, for a few minutes (row 5).
    LoginOutcome outcome;
    outcome.success = true;
    outcome.pendingToken = tokens.newToken();
    NewSession pending;
    pending.idHash = tokens.hash(outcome.pendingToken);
    pending.userID = user->userID;
    pending.clientIP = clientIP;
    pending.mfaVerified = false;
    pending.createdAt = current;
    pending.expiresAt = current + policy.pendingSessionSeconds;
    repository.createSession(pending);
    repository.audit("autenticacion", "Contrasena verificada; falta el segundo factor",
                     clientIP, user->userID, "");
    return outcome;
}

SecondFactorOutcome Authentication::rejectSecondFactor(const SessionRecord& pending,
                                                       const std::string& pendingHash,
                                                       const std::string& clientIP,
                                                       const char* reason)
{
    repository.audit("autenticacion", "Segundo factor rechazado", clientIP, pending.userID, reason);
    const bool lockedNow = repository.recordFailedAttempt(
        pending.userID, policy.maxFailedAttempts, now() + policy.lockSeconds);
    if (lockedNow) {
        repository.deleteSession(pendingHash);
        repository.incident("autenticacion", "Cuenta bloqueada por intentos fallidos",
                            clientIP, pending.userID, "factor=yubikey");
        if (const auto user = repository.findUserByID(pending.userID)) {
            notifyLock(*user, clientIP);
        }
        return {SecondFactorOutcome::Status::Locked, ""};
    }
    return {SecondFactorOutcome::Status::Rejected, ""};
}

SecondFactorOutcome Authentication::verifySecondFactor(const std::string& pendingToken,
                                                       std::string otp,
                                                       const std::string& clientIP)
{
    const std::int64_t current = now();
    if (!isWellFormedToken(pendingToken)) {
        return {SecondFactorOutcome::Status::SessionExpired, ""};
    }
    const std::string pendingHash = tokens.hash(pendingToken);
    const std::optional<SessionRecord> pending = repository.findSession(pendingHash);
    if (!pending || pending->mfaVerified) {
        return {SecondFactorOutcome::Status::SessionExpired, ""};
    }
    if (current >= pending->expiresAt) {
        repository.deleteSession(pendingHash);
        return {SecondFactorOutcome::Status::SessionExpired, ""};
    }

    const std::optional<LoginUser> user = repository.findUserByID(pending->userID);
    if (!user || (user->lockedUntil && *user->lockedUntil > current)) {
        repository.deleteSession(pendingHash);
        return {SecondFactorOutcome::Status::Locked, ""};
    }

    for (char& c : otp) {  // Caps Lock makes the YubiKey type uppercase
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c - 'A' + 'a');
        }
    }
    if (!isModhexOtp(otp)) {
        return rejectSecondFactor(*pending, pendingHash, clientIP, "formato invalido");
    }

    const std::optional<StoredYubikey> key = repository.findYubikey(pending->userID);
    if (!key) {
        return rejectSecondFactor(*pending, pendingHash, clientIP, "usuario sin YubiKey");
    }
    // The OTP must come from THIS user's YubiKey: its public ID first.
    if (otp.compare(0, 12, key->publicID) != 0) {
        return rejectSecondFactor(*pending, pendingHash, clientIP, "OTP de otra YubiKey");
    }

    // Open the secrets with the master key. Failing here means the stored
    // values were altered (or the master key changed): an incident.
    std::optional<std::vector<unsigned char>> aesKey =
        secrets.open(key->aesKeySealed, "yubikey-aes-key:" + key->publicID);
    std::optional<std::vector<unsigned char>> privateID =
        secrets.open(key->privateIDSealed, "yubikey-private-id:" + key->publicID);
    if (!aesKey || !privateID) {
        repository.incident("integridad", "No se pudieron abrir los secretos de una YubiKey",
                            clientIP, pending->userID, "public_id=" + key->publicID);
        return rejectSecondFactor(*pending, pendingHash, clientIP, "secretos no disponibles");
    }

    std::string aesKeyHex = toHex(*aesKey);
    wipe(*aesKey);
    std::optional<DecodedOtp> decoded = decoder.decode(otp, aesKeyHex);
    wipe(aesKeyHex);
    if (!decoded) {
        wipe(*privateID);
        return rejectSecondFactor(*pending, pendingHash, clientIP, "OTP invalido");
    }
    // The private ID inside the block must be the one registered: a second
    // check, besides the CRC, that the OTP comes from this YubiKey.
    const bool samePrivateID = constantTimeEqual(decoded->privateID, *privateID);
    wipe(*privateID);
    wipe(decoded->aesKey);
    wipe(decoded->privateID);
    if (!samePrivateID) {
        return rejectSecondFactor(*pending, pendingHash, clientIP, "private ID distinto");
    }

    // Replay protection (row 21): only OTPs newer than the last accepted
    // one, and the update itself checks it again, so two simultaneous
    // requests with the same OTP cannot both pass.
    if (!isNewer(decoded->counter, decoded->sessionUse, key->lastCounter, key->lastSessionUse) ||
        !repository.advanceOtpCounters(key->keyID, decoded->counter, decoded->sessionUse)) {
        repository.incident("autenticacion", "Intento de reutilizar un OTP", clientIP,
                            pending->userID, "public_id=" + key->publicID);
        return rejectSecondFactor(*pending, pendingHash, clientIP, "OTP reutilizado");
    }

    // Success. The pending session is replaced by a NEW one (rows 3 and 5):
    // whoever knew the pending identifier cannot use the full session.
    repository.resetFailedAttempts(pending->userID);
    repository.deleteSession(pendingHash);

    SecondFactorOutcome outcome;
    outcome.status = SecondFactorOutcome::Status::Success;
    outcome.sessionToken = tokens.newToken();
    NewSession session;
    session.idHash = tokens.hash(outcome.sessionToken);
    session.userID = pending->userID;
    session.clientIP = clientIP;
    session.mfaVerified = true;
    session.createdAt = current;
    session.expiresAt = current + policy.maxSessionSeconds;
    repository.createSession(session);
    repository.audit("autenticacion", "Inicio de sesion exitoso", clientIP, pending->userID, "");
    notifyLogin(*user, clientIP);
    return outcome;
}

std::optional<SessionInfo> Authentication::resolveSession(const std::string& token)
{
    if (!isWellFormedToken(token)) {
        return std::nullopt;
    }
    const std::string idHash = tokens.hash(token);
    const std::optional<SessionRecord> session = repository.findSession(idHash);
    if (!session) {
        return std::nullopt;
    }

    // Expiration is decided here, on the server, whatever the cookie says.
    const std::int64_t current = now();
    const bool expired = current >= session->expiresAt ||
                         (session->mfaVerified &&
                          current - session->lastSeenAt >= policy.idleSessionSeconds) ||
                         !session->userActive;
    if (expired) {
        repository.deleteSession(idHash);
        return std::nullopt;
    }
    if (session->mfaVerified) {
        repository.touchSession(idHash, current);  // restarts the inactivity timer
    }

    SessionInfo info;
    info.userID = session->userID;
    info.username = session->username;
    info.isAdmin = session->isAdmin;
    info.fullyAuthenticated = session->mfaVerified;
    return info;
}

void Authentication::logout(const std::string& token, const std::string& clientIP)
{
    if (!isWellFormedToken(token)) {
        return;
    }
    const std::string idHash = tokens.hash(token);
    const std::optional<SessionRecord> session = repository.findSession(idHash);
    if (!session) {
        return;
    }
    repository.deleteSession(idHash);  // the cookie stops working on the server
    repository.audit("autenticacion", "Cierre de sesion", clientIP, session->userID, "");
}
