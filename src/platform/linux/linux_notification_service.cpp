#include "linux_notification_service.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusReply>

namespace Aero::Platform::Linux {
namespace {
constexpr auto kService = "org.freedesktop.Notifications";
constexpr auto kPath = "/org/freedesktop/Notifications";
constexpr auto kInterface = "org.freedesktop.Notifications";
} // namespace

LinuxNotificationService::LinuxNotificationService(QObject* parent)
    : INotificationService(parent)
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    bus.connect(QString::fromLatin1(kService),
                QString::fromLatin1(kPath),
                QString::fromLatin1(kInterface),
                QStringLiteral("NotificationClosed"),
                this,
                SLOT(onNotificationClosed(uint,uint)));
    bus.connect(QString::fromLatin1(kService),
                QString::fromLatin1(kPath),
                QString::fromLatin1(kInterface),
                QStringLiteral("ActionInvoked"),
                this,
                SLOT(onActionInvoked(uint,QString)));
}

bool LinuxNotificationService::isAvailable() const
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        return false;
    }
    QDBusConnectionInterface* interface = bus.interface();
    return interface != nullptr && interface->isServiceRegistered(QString::fromLatin1(kService));
}

quint32 LinuxNotificationService::post(const NotificationRequest& request)
{
    if (!isAvailable()) {
        setLastError(QStringLiteral("No desktop notification daemon is running"));
        return 0;
    }

    QDBusMessage message = QDBusMessage::createMethodCall(
        QString::fromLatin1(kService),
        QString::fromLatin1(kPath),
        QString::fromLatin1(kInterface),
        QStringLiteral("Notify"));

    QVariantMap hints = request.hints;
    if (!request.applicationIcon.isEmpty() && !hints.contains(QStringLiteral("image-path"))) {
        hints.insert(QStringLiteral("image-path"), request.applicationIcon);
    }

    message.setArguments({
        request.applicationName,
        0u,
        request.applicationIcon,
        request.summary,
        request.body,
        request.actions,
        hints,
        request.expireTimeoutMs,
    });

    const QDBusMessage reply = QDBusConnection::sessionBus().call(message, QDBus::Block, 2000);
    if (reply.type() == QDBusMessage::ErrorMessage) {
        setLastError(reply.errorMessage());
        return 0;
    }

    if (reply.arguments().isEmpty()) {
        return 0;
    }
    return reply.arguments().first().toUInt();
}

void LinuxNotificationService::close(quint32 notificationId)
{
    if (notificationId == 0) {
        return;
    }

    QDBusMessage message = QDBusMessage::createMethodCall(
        QString::fromLatin1(kService),
        QString::fromLatin1(kPath),
        QString::fromLatin1(kInterface),
        QStringLiteral("CloseNotification"));
    message.setArguments({notificationId});
    QDBusConnection::sessionBus().asyncCall(message);
}

void LinuxNotificationService::onNotificationClosed(uint notificationId, uint reason)
{
    emit notificationClosed(notificationId, reason);
}

void LinuxNotificationService::onActionInvoked(uint notificationId, const QString& action)
{
    emit actionInvoked(notificationId, action);
}

} // namespace Aero::Platform::Linux
