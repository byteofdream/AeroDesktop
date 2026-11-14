#pragma once

#include "aero/platform/i_notification_service.h"

namespace Aero::Platform::Linux {

class LinuxNotificationService final : public INotificationService {
    Q_OBJECT
public:
    explicit LinuxNotificationService(QObject* parent = nullptr);

    bool isAvailable() const override;
    quint32 post(const NotificationRequest& request) override;
    void close(quint32 notificationId) override;

private slots:
    void onNotificationClosed(uint notificationId, uint reason);
    void onActionInvoked(uint notificationId, const QString& action);
};

} // namespace Aero::Platform::Linux
