#pragma once

#include "aero/apps/app_context.h"

#include <QObject>
#include <QPointer>
#include <QString>

#include <memory>

namespace Aero::Applications {
class AppView;
}
namespace Aero::Core {
class EventBus;
}
namespace Aero::Platform {
class Platform;
enum class Backend;
}
namespace Aero::Services {
class CalendarStore;
class NotificationBus;
class SearchService;
}
namespace Aero::Settings {
class SettingsManager;
}

namespace Aero::Shell {

class AppSwitcher;
class CalendarPopup;
class ControlCenter;
class DesktopWindow;
class NotificationBanner;
class NotificationCenter;
class ShortcutManager;
class Spotlight;
class WorkspaceOverview;

/// Process-level shell controller.
///
/// It owns the platform backend, persistent stores, services, the desktop root
/// window, every overlay and the translation of application events into
/// compositor windows.  Applications themselves only receive an AppContext.
class Shell : public QObject {
    Q_OBJECT
public:
    explicit Shell(QObject* parent = nullptr);
    ~Shell() override;

    /// Creates the platform, stores, desktop and overlays.  Returns false with
    /// an explanation when the selected backend has no usable services.
    bool initialize(Platform::Backend backend, QString* error = nullptr);

    void show();
    void shutdown();

    bool isInitialized() const;

    Platform::Platform* platform() const;
    Settings::SettingsManager* settings() const;
    Services::NotificationBus* notifications() const;
    Services::CalendarStore* calendar() const;
    Services::SearchService* searchService() const;
    Compositor::WindowManager* windowManager() const;
    DesktopWindow* desktopWindow() const;

    Applications::AppContext appContext() const;

    int launchApplication(const QString& appId, const QString& argument = {});
    void openPath(const QString& path);

    void toggleSpotlight();
    void toggleControlCenter();
    void toggleNotificationCenter();
    void toggleCalendarPopup();
    void toggleWorkspaceOverview();
    void showAppSwitcher(bool reverse);
    void cycleAppSwitcher(bool reverse);
    void commitAppSwitcher();
    void cancelAppSwitcher();

    void lockScreen();
    void requestLogout();
    void requestRestart();
    void requestPowerOff();
    void showAbout();

private:
    void buildDesktop();
    void connectDesktop();
    void buildOverlays();
    void connectServices();
    void connectShortcuts();
    void applySettings();
    void applyAppearance();
    void showNotificationBanner(const Services::Notification& notification);
    void handleLaunchRequest(const QString& appId, const QString& argument);
    void handleOpenRequest(const QString& path);
    void hideAllOverlaysExcept(QWidget* exception = nullptr);
    void openSettingsPage(const QString& page = {});
    void showDockAppMenu(const QString& appId, const QPoint& globalPosition);
    void showDockMenu(const QPoint& globalPosition);
    bool confirm(const QString& title, const QString& text);

    bool m_initialized = false;
    std::unique_ptr<Platform::Platform> m_platform;
    std::unique_ptr<Settings::SettingsManager> m_settings;
    std::unique_ptr<Core::EventBus> m_events;
    std::unique_ptr<Services::NotificationBus> m_notifications;
    std::unique_ptr<Services::CalendarStore> m_calendar;
    std::unique_ptr<Services::SearchService> m_search;

    QPointer<DesktopWindow> m_desktop;
    QPointer<ControlCenter> m_controlCenter;
    QPointer<Spotlight> m_spotlight;
    QPointer<NotificationCenter> m_notificationCenter;
    QPointer<NotificationBanner> m_notificationBanner;
    QPointer<CalendarPopup> m_calendarPopup;
    QPointer<WorkspaceOverview> m_workspaceOverview;
    QPointer<AppSwitcher> m_appSwitcher;
    QPointer<ShortcutManager> m_shortcutManager;

    Applications::AppContext m_context;
    bool m_switcherOpen = false;
};

} // namespace Aero::Shell
