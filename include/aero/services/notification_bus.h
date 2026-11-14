#pragma once

#include <QDateTime>
#include <QObject>
#include <QStringList>
#include <QVariantMap>

#include <memory>

namespace Aero::Core {
class JsonConfig;
}
namespace Aero::Settings {
class SettingsManager;
}

namespace Aero::Services {

struct Notification {
    QString id;
    QString appId;
    QString appName;
    QString title;
    QString body;
    QString iconName;
    QDateTime timestamp;
    bool read = false;
    QStringList actions;

    QVariantMap toVariantMap() const;
    static Notification fromVariantMap(const QVariantMap& map);
};

/// Authoritative notification store for the Aero shell.
///
/// The bus owns history, read state and Do Not Disturb policy.  It persists to
/// the Aero data directory and is independent from the host notification
/// daemon, so the shell works identically on DreamyOS.
class NotificationBus : public QObject {
    Q_OBJECT
public:
    explicit NotificationBus(Settings::SettingsManager* settings, QObject* parent = nullptr);
    ~NotificationBus() override;

    QList<Notification> notifications() const;
    int unreadCount() const;
    int count() const;

    void post(const Notification& notification);
    void post(const QString& appId,
              const QString& appName,
              const QString& title,
              const QString& body,
              const QString& iconName = {});

    void dismiss(const QString& id);
    void clearAll();
    void markAllRead();
    void markRead(const QString& id);

    bool doNotDisturb() const;
    void setDoNotDisturb(bool enabled);

signals:
    void posted(const Notification& notification);
    void bannerRequested(const Notification& notification);
    void dismissed(const QString& id);
    void changed();
    void unreadCountChanged(int count);

private:
    void load();
    void save();
    void enforceHistoryLimit();

    std::unique_ptr<Core::JsonConfig> m_config;
    Settings::SettingsManager* m_settings = nullptr;
    QList<Notification> m_notifications;
};

} // namespace Aero::Services

Q_DECLARE_METATYPE(Aero::Services::Notification)
