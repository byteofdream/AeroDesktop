#pragma once

#include "aero/services/notification_bus.h"
#include "aero/shell/overlay.h"

#include <QPointer>

class QLabel;
class QScrollArea;
class QTimer;
class QVBoxLayout;

namespace Aero::Services {
class NotificationBus;
}
namespace Aero::Settings {
class SettingsManager;
}

namespace Aero::Theme {
class ToggleSwitch;
}

namespace Aero::Shell {

/// Notification history sidebar.  Notifications are grouped in reverse
/// chronological order with unread emphasis and per-item dismissal.
class NotificationCenter : public Overlay {
    Q_OBJECT
public:
    NotificationCenter(Services::NotificationBus* bus,
                       Settings::SettingsManager* settings,
                       QWidget* parent = nullptr);

    void refresh();

signals:
    void notificationActivated(const QString& appId);
    void settingsRequested();

protected:
    void prepareShow() override;
    void paintEvent(QPaintEvent* event) override;

private:
    void build();
    void rebuildCards();

    Services::NotificationBus* m_bus = nullptr;
    Settings::SettingsManager* m_settings = nullptr;
    QVBoxLayout* m_cardsLayout = nullptr;
    QScrollArea* m_scroll = nullptr;
    QLabel* m_emptyLabel = nullptr;
    QLabel* m_summary = nullptr;
    Theme::ToggleSwitch* m_dnd = nullptr;
    bool m_updating = false;
};

/// Transient banner shown when the notification bus requests one.  It remains
/// a child of the desktop so it follows the shell across workspaces and never
/// creates an extra top-level window.
class NotificationBanner : public Graphics::GlassPanel {
    Q_OBJECT
public:
    explicit NotificationBanner(QWidget* parent = nullptr);

    void showNotification(const Services::Notification& notification, bool showBody);
    void hideBanner();

signals:
    void activated(const QString& appId, const QString& notificationId);

protected:
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    void updateTimer();

    Services::Notification m_notification;
    QLabel* m_appLabel = nullptr;
    QLabel* m_titleLabel = nullptr;
    QLabel* m_bodyLabel = nullptr;
    QTimer* m_timer = nullptr;
    bool m_hovered = false;
};

} // namespace Aero::Shell
