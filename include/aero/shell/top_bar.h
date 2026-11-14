#pragma once

#include "aero/graphics/glass_panel.h"

#include <QPoint>
#include <QSize>

class QAbstractButton;
class QLabel;
class QTimer;

namespace Aero::Compositor {
class WindowManager;
}
namespace Aero::Platform {
class Platform;
}
namespace Aero::Services {
class NotificationBus;
}
namespace Aero::Settings {
class SettingsManager;
}

namespace Aero::Shell {

class TopBar : public Graphics::GlassPanel {
    Q_OBJECT
public:
    TopBar(Platform::Platform* platform,
           Settings::SettingsManager* settings,
           Services::NotificationBus* notifications,
           Compositor::WindowManager* windowManager,
           QWidget* parent = nullptr);

    QSize sizeHint() const override;

    void refresh();
    void setUnreadCount(int count);

signals:
    void calendarRequested(const QPoint& globalAnchor);
    void controlCenterRequested(const QPoint& globalAnchor);
    void notificationCenterRequested(const QPoint& globalAnchor);
    void spotlightRequested();
    void workspaceOverviewRequested();
    void settingsRequested();
    void aboutRequested();
    void lockRequested();
    void logoutRequested();
    void restartRequested();
    void powerOffRequested();

protected:
    void resizeEvent(QResizeEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    void build();
    void refreshStatus();
    void refreshApplicationName();
    void refreshWorkspaces();
    void showSystemMenu();
    void updateClock();
    void positionBadge();

    Platform::Platform* m_platform = nullptr;
    Settings::SettingsManager* m_settings = nullptr;
    Services::NotificationBus* m_notifications = nullptr;
    Compositor::WindowManager* m_windowManager = nullptr;

    QLabel* m_appName = nullptr;
    QAbstractButton* m_menuButton = nullptr;
    QAbstractButton* m_clock = nullptr;
    QLabel* m_badge = nullptr;
    QTimer* m_clockTimer = nullptr;
    QTimer* m_statusTimer = nullptr;

    class WorkspaceDots;
    WorkspaceDots* m_workspaceDots = nullptr;

    class StatusButton;
    StatusButton* m_networkButton = nullptr;
    StatusButton* m_volumeButton = nullptr;
    StatusButton* m_batteryButton = nullptr;
    StatusButton* m_bellButton = nullptr;

    int m_unreadCount = 0;
};

} // namespace Aero::Shell
