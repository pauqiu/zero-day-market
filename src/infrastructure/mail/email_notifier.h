#ifndef EMAIL_NOTIFIER_H
#define EMAIL_NOTIFIER_H

#include <cstdint>
#include <functional>
#include <string>

#include "application/notifications/notifier.h"

// Notifications by email through the course server. The configuration is
// read on the first message. Never throws: a failure returns false and is
// written to Apache's error log (without secrets).
class EmailNotifier : public Notifier {
    public:
        using Clock = std::function<std::int64_t()>;
        explicit EmailNotifier(std::string configPath, Clock now);

        bool sendVerification(const std::string& to, const std::string& username,
                              const std::string& token) override;
        bool sendLoginNotice(const std::string& to, const std::string& username,
                             std::int64_t when, const std::string& clientIP) override;
        bool sendLockNotice(const std::string& to, const std::string& username,
                            std::int64_t when, const std::string& clientIP) override;

    private:
        bool deliver(const std::string& to, const std::string& subject,
                     const std::function<std::string(const std::string& appUrl)>& body);

        std::string configPath;
        Clock now;
};

#endif // EMAIL_NOTIFIER_H
