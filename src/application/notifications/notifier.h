#ifndef NOTIFIER_H
#define NOTIFIER_H

#include <cstdint>
#include <string>

// Port: messages to users (HU-07, HU-13). The use cases decide WHEN to
// notify; the implementation decides how (email). Every method returns
// false if the message could not be delivered: a failed notification never
// undoes the operation that caused it (RF-4.34); the caller logs it.
class Notifier {
    public:
        virtual ~Notifier() = default;

        // Link to activate a new account. The token travels only in this message.
        virtual bool sendVerification(const std::string& to, const std::string& username,
                                      const std::string& token) = 0;

        // A successful login (RF-2.31).
        virtual bool sendLoginNotice(const std::string& to, const std::string& username,
                                     std::int64_t when, const std::string& clientIP) = 0;

        // The account was locked by failed attempts (RF-2.32).
        virtual bool sendLockNotice(const std::string& to, const std::string& username,
                                    std::int64_t when, const std::string& clientIP) = 0;
};

#endif // NOTIFIER_H
