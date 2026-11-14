#pragma once

#include "aero/platform/platform_types.h"
#include "aero/platform/platform_service.h"

namespace Aero::Platform {

/// Bridges Aero notifications to the host notification service (if any).
/// Aero's own NotificationBus is authoritative inside the shell even when the
/// host has no notification daemon.
class INotificationService : public PlatformService {
    Q_OBJECT
public:
    explicit INotificationService(QObject* parent = nullptr)
        : PlatformService(parent)
    {
        setCapability(Capability::Notifications);
    }

    /// Returns the platform notification id, or 0 when posting failed.
    virtual quint32 post(const NotificationRequest& request) = 0;
    virtual void close(quint32 notificationId) = 0;

signals:
    void notificationClosed(quint32 notificationId, quint32 reason);
    void actionInvoked(quint32 notificationId, const QString& action);
};

} // namespace Aero::Platform
