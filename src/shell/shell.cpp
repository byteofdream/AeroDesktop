#include "aero/shell/shell.h"

#include "aero/apps/app_registry.h"
#include "aero/apps/app_view.h"
#include "aero/compositor/window.h"
#include "aero/compositor/window_manager.h"
#include "aero/compositor/workspace_model.h"
#include "aero/core/event_bus.h"
#include "aero/core/logging.h"
#include "aero/core/paths.h"
#include "aero/platform/i_file_system.h"
#include "aero/platform/i_notification_service.h"
#include "aero/platform/i_power_manager.h"
#include "aero/platform/i_session_manager.h"
#include "aero/platform/platform.h"
#include "aero/services/calendar_store.h"
#include "aero/services/notification_bus.h"
#include "aero/services/search_service.h"
#include "aero/settings/settings_keys.h"
#include "aero/settings/settings_manager.h"
#include "aero/shell/app_switcher.h"
#include "aero/shell/calendar_popup.h"
#include "aero/shell/control_center.h"
#include "aero/shell/desktop_window.h"
#include "aero/shell/dock.h"
#include "aero/shell/notification_center.h"
#include "aero/shell/shortcut_manager.h"
#include "aero/shell/spotlight.h"
#include "aero/shell/top_bar.h"
#include "aero/shell/workspace_overview.h"
#include "aero/theme/icon_factory.h"
#include "aero/theme/style.h"
#include "aero/theme/theme.h"

#include <QAction>
#include <QApplication>
#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QKeySequence>
#include <QMenu>
#include <QMessageBox>
#include <QPointer>
#include <QTimer>
#include <QWidget>

#include <algorithm>
#include <utility>

namespace Aero::Shell {
namespace {

QString key(const char* value)
{
    return QString::fromLatin1(value);
}

QString archiveSuffixes()
{
    return QStringLiteral("zip tar gz tgz bz2 tbz xz txz 7z rar zst");
}

QString textSuffixes()
{
    return QStringLiteral("txt md markdown log ini conf cfg json yaml yml toml cpp cc cxx h hpp "
                          "c py js ts tsx jsx css scss html htm xml sh bash zsh fish csv");
}

bool matchesSuffix(const QString& suffix, const QString& list)
{
    if (suffix.isEmpty()) {
        return false;
    }
    return list.split(QLatin1Char(' ')).contains(suffix, Qt::CaseInsensitive);
}

QIcon appIconFor(const QString& appId)
{
    return Theme::IconFactory::appIcon(appId, 64);
}

} // namespace

Shell::Shell(QObject* parent)
    : QObject(parent)
{
    Applications::AppRegistry::registerBuiltInApplications();
}

Shell::~Shell()
{
    shutdown();
}

bool Shell::initialize(Platform::Backend backend, QString* error)
{
    if (m_initialized) {
        return true;
    }

    if (error != nullptr) {
        error->clear();
    }

    QString pathError;
    if (!Core::Paths::ensureDirectories(&pathError)) {
        qCWarning(aeroLog) << "Could not create Aero directories:" << pathError;
    }

    m_platform = Platform::createPlatform(nullptr, backend);
    if (m_platform == nullptr) {
        if (error != nullptr) {
            *error = QStringLiteral("The selected platform backend could not be created.");
        }
        return false;
    }

    if (backend == Platform::Backend::DreamyOS) {
        if (error != nullptr) {
            *error = QStringLiteral(
                "The DreamyOS platform backend is an interface scaffold in this build. "
                "It intentionally does not provide window, input, filesystem or session services. "
                "Run with --backend linux for the development backend, or implement "
                "src/platform/dreamyos before enabling AERO_BUILD_DREAMYOS_BACKEND.");
        }
        m_platform.reset();
        return false;
    }

    Platform::IFileSystem* fileSystem = m_platform->fileSystem();
    if (fileSystem == nullptr || !fileSystem->isAvailable()) {
        if (error != nullptr) {
            *error = QStringLiteral("The platform backend did not provide a usable filesystem service.");
        }
        m_platform.reset();
        return false;
    }

    m_settings = std::make_unique<Settings::SettingsManager>();
    QString settingsError;
    if (!m_settings->load(&settingsError) && !settingsError.isEmpty()) {
        qCWarning(aeroLog) << "Settings were not loaded:" << settingsError;
    }

    m_events = std::make_unique<Core::EventBus>();
    m_notifications = std::make_unique<Services::NotificationBus>(m_settings.get());
    m_calendar = std::make_unique<Services::CalendarStore>();
    m_search = std::make_unique<Services::SearchService>(fileSystem);

    QStringList roots;
    const QString home = fileSystem->homePath();
    if (!home.isEmpty()) {
        roots.append(home);
    }
    const QList<Platform::StandardLocation> locations = {
        Platform::StandardLocation::Desktop,
        Platform::StandardLocation::Documents,
        Platform::StandardLocation::Downloads,
    };
    for (Platform::StandardLocation location : locations) {
        const QString path = fileSystem->standardLocation(location);
        if (!path.isEmpty() && !roots.contains(path)) {
            roots.append(path);
        }
    }
    if (roots.isEmpty()) {
        roots.append(QDir::homePath());
    }
    m_search->setSearchRoots(roots);

    m_context.platform = m_platform.get();
    m_context.settings = m_settings.get();
    m_context.notifications = m_notifications.get();
    m_context.calendar = m_calendar.get();
    m_context.events = m_events.get();

    buildDesktop();
    if (m_desktop == nullptr) {
        if (error != nullptr) {
            *error = QStringLiteral("The Aero desktop window could not be created.");
        }
        return false;
    }
    m_context.windowManager = m_desktop->windowManager();

    buildOverlays();
    connectDesktop();
    connectServices();
    connectShortcuts();

    m_events->subscribe(Core::EventBus::LaunchApplication, [this](const QVariantMap& payload) {
        handleLaunchRequest(payload.value(QStringLiteral("appId")).toString(),
                            payload.value(QStringLiteral("argument")).toString());
    });
    m_events->subscribe(Core::EventBus::OpenFile, [this](const QVariantMap& payload) {
        handleOpenRequest(payload.value(QStringLiteral("path")).toString());
    });

    applySettings();

    m_initialized = true;
    qCInfo(aeroLog) << "Aero shell initialized with" << m_platform->backendDisplayName()
                    << "backend";
    return true;
}

void Shell::buildDesktop()
{
    m_desktop = new DesktopWindow(m_platform.get(), m_settings.get(), m_notifications.get());
    m_desktop->setWindowTitle(QStringLiteral("Aero Desktop"));
    m_desktop->setWindowIcon(Theme::IconFactory::appIcon(QStringLiteral("aero-about"), 64));
}

void Shell::buildOverlays()
{
    if (m_desktop == nullptr) {
        return;
    }

    m_controlCenter = new ControlCenter(m_platform.get(), m_settings.get(),
                                       m_notifications.get(), m_desktop);
    m_desktop->addOverlay(m_controlCenter, OverlayPlacement::TopRight);

    m_spotlight = new Spotlight(m_search.get(), m_desktop);
    m_desktop->addOverlay(m_spotlight, OverlayPlacement::Center);

    m_notificationCenter = new NotificationCenter(m_notifications.get(), m_settings.get(), m_desktop);
    m_desktop->addOverlay(m_notificationCenter, OverlayPlacement::TopRight);

    m_calendarPopup = new CalendarPopup(m_calendar.get(), m_desktop);
    m_desktop->addOverlay(m_calendarPopup, OverlayPlacement::TopRight);

    m_workspaceOverview = new WorkspaceOverview(m_desktop->windowManager(), m_settings.get(), m_desktop);
    m_desktop->addOverlay(m_workspaceOverview, OverlayPlacement::FullScreen);

    m_appSwitcher = new AppSwitcher(m_desktop->windowManager(), m_desktop);
    m_desktop->addOverlay(m_appSwitcher, OverlayPlacement::Center);

    m_notificationBanner = new NotificationBanner(m_desktop);
    m_notificationBanner->hide();

    m_shortcutManager = new ShortcutManager(m_settings.get(), m_desktop, this);
}

void Shell::connectDesktop()
{
    if (m_desktop == nullptr) {
        return;
    }

    if (TopBar* topBar = m_desktop->topBar()) {
        connect(topBar, &TopBar::calendarRequested, this, [this](const QPoint&) {
            toggleCalendarPopup();
        });
        connect(topBar, &TopBar::controlCenterRequested, this, [this](const QPoint&) {
            toggleControlCenter();
        });
        connect(topBar, &TopBar::notificationCenterRequested, this, [this](const QPoint&) {
            toggleNotificationCenter();
        });
        connect(topBar, &TopBar::spotlightRequested, this, &Shell::toggleSpotlight);
        connect(topBar, &TopBar::workspaceOverviewRequested, this, &Shell::toggleWorkspaceOverview);
        connect(topBar, &TopBar::settingsRequested, this, [this]() { openSettingsPage(); });
        connect(topBar, &TopBar::aboutRequested, this, &Shell::showAbout);
        connect(topBar, &TopBar::lockRequested, this, &Shell::lockScreen);
        connect(topBar, &TopBar::logoutRequested, this, &Shell::requestLogout);
        connect(topBar, &TopBar::restartRequested, this, &Shell::requestRestart);
        connect(topBar, &TopBar::powerOffRequested, this, &Shell::requestPowerOff);
    }

    if (Dock* dock = m_desktop->dock()) {
        connect(dock, &Dock::appActivated, this, [this, dock](const QString& appId) {
            if (m_desktop == nullptr || m_desktop->windowManager() == nullptr) {
                return;
            }
            Compositor::WindowManager* wm = m_desktop->windowManager();
            const QList<Compositor::AeroWindow*> windows = wm->windowsForApplication(appId);
            if (windows.isEmpty()) {
                const int id = launchApplication(appId);
                if (id >= 0 && dock != nullptr) {
                    dock->bounce(appId);
                }
                return;
            }

            Compositor::AeroWindow* target = windows.last();
            if (target == nullptr) {
                return;
            }
            if (wm->activeWindowId() == target->id() && windows.size() == 1
                && target->state() != Compositor::AeroWindow::State::Minimized) {
                wm->minimizeWindow(target->id());
                return;
            }
            if (target->state() == Compositor::AeroWindow::State::Minimized) {
                wm->restoreWindow(target->id());
            } else {
                wm->activateWindow(target->id());
            }
        });
        connect(dock, &Dock::appMenuRequested, this, &Shell::showDockAppMenu);
        connect(dock, &Dock::dockMenuRequested, this, &Shell::showDockMenu);
    }

    connect(m_desktop, &DesktopWindow::openPathRequested, this, &Shell::openPath);
    connect(m_desktop, &DesktopWindow::desktopContextMenuRequested, this,
            [this](const QPoint& position) {
                QMenu menu;
                QAction* terminal = menu.addAction(
                    Theme::IconFactory::icon(QStringLiteral("terminal"), 16),
                    QStringLiteral("Open Terminal"));
                QAction* settings = menu.addAction(
                    Theme::IconFactory::icon(QStringLiteral("gear"), 16),
                    QStringLiteral("Desktop & Wallpaper Settings…"));
                QAction* displays = menu.addAction(
                    Theme::IconFactory::icon(QStringLiteral("display"), 16),
                    QStringLiteral("Display Settings…"));
                menu.addSeparator();
                QAction* overview = menu.addAction(
                    Theme::IconFactory::icon(QStringLiteral("grid"), 16),
                    QStringLiteral("Workspace Overview"));
                QAction* showDesktop = menu.addAction(
                    Theme::IconFactory::icon(QStringLiteral("desktop"), 16),
                    QStringLiteral("Show Desktop"));
                menu.addSeparator();
                QAction* about = menu.addAction(
                    Theme::IconFactory::icon(QStringLiteral("info"), 16),
                    QStringLiteral("About Aero Desktop"));

                QAction* chosen = menu.exec(position);
                if (chosen == terminal) {
                    launchApplication(Applications::AppIds::Terminal);
                } else if (chosen == settings) {
                    openSettingsPage(QStringLiteral("Desktop"));
                } else if (chosen == displays) {
                    openSettingsPage(QStringLiteral("Displays"));
                } else if (chosen == overview) {
                    toggleWorkspaceOverview();
                } else if (chosen == showDesktop) {
                    if (m_desktop->windowManager() != nullptr) {
                        m_desktop->windowManager()->showDesktop();
                    }
                } else if (chosen == about) {
                    showAbout();
                }
            });

    if (Compositor::WindowManager* wm = m_desktop->windowManager()) {
        connect(wm, &Compositor::WindowManager::activeWindowChanged, this, [this](int id) {
            if (m_desktop == nullptr || m_desktop->windowManager() == nullptr) {
                return;
            }
            Compositor::AeroWindow* window = m_desktop->windowManager()->windowById(id);
            if (window == nullptr) {
                return;
            }
            if (auto* view = qobject_cast<Applications::AppView*>(window->contentWidget())) {
                view->activate();
            }
        });
    }

    if (m_controlCenter != nullptr) {
        connect(m_controlCenter, &ControlCenter::settingsRequested, this, [this]() {
            openSettingsPage();
        });
        connect(m_controlCenter, &ControlCenter::lockRequested, this, &Shell::lockScreen);
        connect(m_controlCenter, &ControlCenter::displaySettingsRequested, this, [this]() {
            openSettingsPage(QStringLiteral("Displays"));
        });
    }

    if (m_spotlight != nullptr) {
        connect(m_spotlight, &Spotlight::applicationRequested, this, [this](const QString& appId) {
            launchApplication(appId);
        });
        connect(m_spotlight, &Spotlight::fileRequested, this, &Shell::openPath);
        connect(m_spotlight, &Spotlight::actionRequested, this, [this](const QString& action) {
            if (action == QStringLiteral("show-desktop")) {
                if (m_desktop != nullptr && m_desktop->windowManager() != nullptr) {
                    m_desktop->windowManager()->showDesktop();
                }
            } else if (action == QStringLiteral("workspace-overview")) {
                toggleWorkspaceOverview();
            } else if (action == QStringLiteral("control-center")) {
                toggleControlCenter();
            } else if (action == QStringLiteral("settings")) {
                openSettingsPage();
            } else if (action == QStringLiteral("lock-screen")) {
                lockScreen();
            } else if (action == QStringLiteral("about")) {
                showAbout();
            }
        });
    }

    if (m_notificationCenter != nullptr) {
        connect(m_notificationCenter, &NotificationCenter::notificationActivated, this,
                [this](const QString& appId) {
                    if (Applications::AppRegistry::instance().contains(appId)) {
                        launchApplication(appId);
                    } else {
                        openSettingsPage(QStringLiteral("Notifications"));
                    }
                });
        connect(m_notificationCenter, &NotificationCenter::settingsRequested, this,
                [this]() { openSettingsPage(QStringLiteral("Notifications")); });
    }

    if (m_calendarPopup != nullptr) {
        connect(m_calendarPopup, &CalendarPopup::openCalendarRequested, this,
                [this](const QDate& date) {
                    launchApplication(Applications::AppIds::Calendar,
                                      date.toString(Qt::ISODate));
                });
        connect(m_calendarPopup, &CalendarPopup::newEventRequested, this,
                [this](const QDate& date) {
                    launchApplication(Applications::AppIds::Calendar,
                                      date.toString(Qt::ISODate));
                });
    }

    if (m_workspaceOverview != nullptr) {
        connect(m_workspaceOverview, &WorkspaceOverview::workspaceActivated, this,
                [this](int index) {
                    if (m_desktop != nullptr && m_desktop->windowManager() != nullptr) {
                        m_desktop->windowManager()->switchWorkspace(index);
                    }
                });
    }

    if (m_appSwitcher != nullptr) {
        connect(m_appSwitcher, &AppSwitcher::windowChosen, this, [this](int id) {
            if (m_desktop != nullptr && m_desktop->windowManager() != nullptr) {
                m_desktop->windowManager()->activateWindow(id);
            }
        });
        connect(m_appSwitcher, &AppSwitcher::selectionCancelled, this, [this]() {
            m_switcherOpen = false;
        });
    }

    if (m_notificationBanner != nullptr) {
        connect(m_notificationBanner, &NotificationBanner::activated, this,
                [this](const QString& appId, const QString& notificationId) {
                    if (m_notifications != nullptr && !notificationId.isEmpty()) {
                        m_notifications->markRead(notificationId);
                    }
                    if (!appId.isEmpty() && Applications::AppRegistry::instance().contains(appId)) {
                        launchApplication(appId);
                    } else if (Applications::AppRegistry::instance().contains(
                                   Applications::AppIds::About)) {
                        // A notification without an application identity is shown
                        // in the notification center instead of launching a random app.
                        toggleNotificationCenter();
                    }
                });
    }
}

void Shell::connectServices()
{
    if (m_notifications != nullptr) {
        connect(m_notifications.get(), &Services::NotificationBus::bannerRequested, this,
                &Shell::showNotificationBanner);
    }

    if (m_settings != nullptr) {
        connect(m_settings.get(), &Settings::SettingsManager::appearanceChanged,
                this, &Shell::applySettings);
        connect(m_settings.get(), &Settings::SettingsManager::dockChanged, this, [this]() {
            if (m_desktop != nullptr) {
                m_desktop->applySettings();
            }
        });
        connect(m_settings.get(), &Settings::SettingsManager::shortcutsChanged, this, [this]() {
            if (m_shortcutManager != nullptr) {
                m_shortcutManager->reload();
            }
        });
        connect(m_settings.get(), &Settings::SettingsManager::settingChanged, this,
                [this](const QString& changedKey, const QVariant&) {
                    if (changedKey == key(Settings::Keys::AppearanceMode)
                        || changedKey == key(Settings::Keys::AccentColor)
                        || changedKey == key(Settings::Keys::FontScale)
                        || changedKey == key(Settings::Keys::ReducedMotion)
                        || changedKey == key(Settings::Keys::HighContrast)) {
                        applyAppearance();
                    }
                });
    }

    if (m_calendar != nullptr && m_calendarPopup != nullptr) {
        connect(m_calendar.get(), &Services::CalendarStore::eventsChanged, this, [this]() {
            if (m_calendarPopup != nullptr && m_calendarPopup->isVisible()) {
                m_calendarPopup->refresh();
            }
        });
    }

    // The host notification daemon is an optional transport, never the source
    // of truth.  Bridging is opt-in so a desktop session does not show every
    // Aero notification twice.
    if (qEnvironmentVariableIsSet("AERO_BRIDGE_NOTIFICATIONS") && m_platform != nullptr
        && m_notifications != nullptr) {
        Platform::INotificationService* host = m_platform->notificationService();
        if (host != nullptr && host->isAvailable()) {
            connect(m_notifications.get(), &Services::NotificationBus::posted, this,
                    [host](const Services::Notification& notification) {
                        Platform::NotificationRequest request;
                        request.applicationName = notification.appName;
                        request.applicationIcon = notification.iconName;
                        request.summary = notification.title;
                        request.body = notification.body;
                        request.expireTimeoutMs = 6000;
                        host->post(request);
                    });
        }
    }
}

void Shell::connectShortcuts()
{
    if (m_shortcutManager == nullptr) {
        return;
    }

    connect(m_shortcutManager, &ShortcutManager::actionTriggered, this,
            [this](const QString& action) {
                if (action == QStringLiteral("spotlight")) {
                    toggleSpotlight();
                } else if (action == QStringLiteral("control-center")) {
                    toggleControlCenter();
                } else if (action == QStringLiteral("notification-center")) {
                    toggleNotificationCenter();
                } else if (action == QStringLiteral("workspace-overview")) {
                    toggleWorkspaceOverview();
                } else if (action == QStringLiteral("workspace-next")) {
                    if (m_desktop != nullptr && m_desktop->windowManager() != nullptr) {
                        m_desktop->windowManager()->switchWorkspaceRelative(1);
                    }
                } else if (action == QStringLiteral("workspace-previous")) {
                    if (m_desktop != nullptr && m_desktop->windowManager() != nullptr) {
                        m_desktop->windowManager()->switchWorkspaceRelative(-1);
                    }
                } else if (action == QStringLiteral("window-close")) {
                    if (m_desktop != nullptr && m_desktop->windowManager() != nullptr) {
                        m_desktop->windowManager()->closeActiveWindow();
                    }
                } else if (action == QStringLiteral("window-minimize")) {
                    if (m_desktop != nullptr && m_desktop->windowManager() != nullptr) {
                        m_desktop->windowManager()->minimizeActiveWindow();
                    }
                } else if (action == QStringLiteral("window-maximize")) {
                    if (m_desktop != nullptr && m_desktop->windowManager() != nullptr) {
                        Compositor::WindowManager* wm = m_desktop->windowManager();
                        const int id = wm->activeWindowId();
                        if (id >= 0) {
                            wm->toggleMaximize(id);
                        }
                    }
                } else if (action == QStringLiteral("show-desktop")) {
                    if (m_desktop != nullptr && m_desktop->windowManager() != nullptr) {
                        m_desktop->windowManager()->showDesktop();
                    }
                } else if (action == QStringLiteral("lock-screen")) {
                    lockScreen();
                }
            });

    connect(m_shortcutManager, &ShortcutManager::appSwitcherStarted, this,
            [this](bool reverse) { showAppSwitcher(reverse); });
    connect(m_shortcutManager, &ShortcutManager::appSwitcherCycled, this,
            [this](bool reverse) { cycleAppSwitcher(reverse); });
    connect(m_shortcutManager, &ShortcutManager::appSwitcherCommitted, this,
            &Shell::commitAppSwitcher);
    connect(m_shortcutManager, &ShortcutManager::appSwitcherCancelled, this,
            &Shell::cancelAppSwitcher);
}

void Shell::applySettings()
{
    if (m_settings == nullptr) {
        return;
    }
    applyAppearance();
    if (m_desktop != nullptr) {
        m_desktop->applySettings();
    }
    if (m_shortcutManager != nullptr) {
        m_shortcutManager->reload();
    }
}

void Shell::applyAppearance()
{
    if (m_settings == nullptr) {
        return;
    }

    Theme::ThemeManager& theme = Theme::ThemeManager::instance();
    theme.setAccentColor(QColor(m_settings->accentColor()));
    theme.setAppearance(Theme::ThemeManager::appearanceFromString(
        m_settings->appearanceMode(), Theme::Appearance::Auto));
    theme.setFontScale(m_settings->fontScale());
    theme.setReducedMotion(m_settings->reducedMotion());

    if (auto* application = qobject_cast<QApplication*>(QCoreApplication::instance())) {
        Theme::Style::install(*application);
    }

    if (m_events != nullptr) {
        QVariantMap payload;
        payload.insert(QStringLiteral("appearance"), m_settings->appearanceMode());
        payload.insert(QStringLiteral("accent"), m_settings->accentColor());
        payload.insert(QStringLiteral("dark"), theme.isDark());
        m_events->publish(Core::EventBus::ThemeChanged, payload);
    }
}

void Shell::showNotificationBanner(const Services::Notification& notification)
{
    if (m_notificationBanner == nullptr || m_desktop == nullptr) {
        return;
    }
    const bool preview = m_settings == nullptr
        || m_settings->value(key(Settings::Keys::NotificationsPreview), true).toBool();
    const int width = qMin(400, qMax(280, m_desktop->width() - 48));
    m_notificationBanner->resize(width, 122);
    const int top = m_desktop->topBarRect().bottom() + 14;
    m_notificationBanner->move(qMax(24, m_desktop->width() - width - 24), top);
    m_notificationBanner->showNotification(notification, preview);
}

void Shell::show()
{
    if (m_desktop == nullptr) {
        return;
    }
    m_desktop->show();
    m_desktop->raise();
    m_desktop->activateWindow();
}

void Shell::shutdown()
{
    if (!m_initialized && m_desktop == nullptr) {
        return;
    }

    if (m_settings != nullptr) {
        m_settings->flush();
    }

    if (m_shortcutManager != nullptr) {
        delete m_shortcutManager;
        m_shortcutManager = nullptr;
    }

    if (m_search != nullptr) {
        m_search->cancel();
    }

    if (m_desktop != nullptr) {
        m_desktop->hide();
        delete m_desktop;
        m_desktop = nullptr;
    }

    m_controlCenter = nullptr;
    m_spotlight = nullptr;
    m_notificationCenter = nullptr;
    m_notificationBanner = nullptr;
    m_calendarPopup = nullptr;
    m_workspaceOverview = nullptr;
    m_appSwitcher = nullptr;

    m_context = {};
    m_initialized = false;
}

bool Shell::isInitialized() const
{
    return m_initialized;
}

Platform::Platform* Shell::platform() const
{
    return m_platform.get();
}

Settings::SettingsManager* Shell::settings() const
{
    return m_settings.get();
}

Services::NotificationBus* Shell::notifications() const
{
    return m_notifications.get();
}

Services::CalendarStore* Shell::calendar() const
{
    return m_calendar.get();
}

Services::SearchService* Shell::searchService() const
{
    return m_search.get();
}

Compositor::WindowManager* Shell::windowManager() const
{
    return m_desktop != nullptr ? m_desktop->windowManager() : nullptr;
}

DesktopWindow* Shell::desktopWindow() const
{
    return m_desktop;
}

Applications::AppContext Shell::appContext() const
{
    return m_context;
}

int Shell::launchApplication(const QString& appId, const QString& argument)
{
    if (m_desktop == nullptr || m_desktop->windowManager() == nullptr) {
        return -1;
    }
    const Applications::AppDefinition* definition =
        Applications::AppRegistry::instance().find(appId);
    if (definition == nullptr) {
        if (m_notifications != nullptr) {
            m_notifications->post(QStringLiteral("aero.desktop"),
                                  QStringLiteral("Aero Desktop"),
                                  QStringLiteral("Application not found"),
                                  QStringLiteral("No installed application has the id “%1”.").arg(appId));
        }
        return -1;
    }

    Compositor::WindowManager* wm = m_desktop->windowManager();

    if (definition->singleInstance) {
        const QList<Compositor::AeroWindow*> existing = wm->windowsForApplication(appId);
        if (!existing.isEmpty()) {
            Compositor::AeroWindow* window = existing.last();
            if (window == nullptr) {
                return -1;
            }
            if (auto* view = qobject_cast<Applications::AppView*>(window->contentWidget())) {
                if (!argument.isEmpty()) {
                    view->setLaunchArgument(argument);
                }
                view->activate();
            }
            if (window->state() == Compositor::AeroWindow::State::Minimized) {
                wm->restoreWindow(window->id());
            } else {
                wm->activateWindow(window->id());
            }
            return window->id();
        }
    }

    auto* view = Applications::AppRegistry::instance().createView(appId, m_context, nullptr);
    if (view == nullptr) {
        if (m_notifications != nullptr) {
            m_notifications->post(QStringLiteral("aero.desktop"),
                                  QStringLiteral("Aero Desktop"),
                                  QStringLiteral("Could not start application"),
                                  QStringLiteral("%1 did not provide a usable view.").arg(definition->name));
        }
        return -1;
    }

    if (!argument.isEmpty()) {
        view->setLaunchArgument(argument);
    }

    Compositor::WindowOptions options;
    options.applicationId = appId;
    options.title = definition->name;
    options.subtitle = definition->description;
    options.icon = appIconFor(appId);
    options.content = view;
    options.defaultSize = definition->defaultSize;
    options.minimumSize = definition->minimumSize;
    options.startMaximized = definition->startMaximized;
    options.closeValidator = [view]() { return view != nullptr && view->canClose(); };

    const int id = wm->openWindow(options);
    if (id < 0) {
        delete view;
        return -1;
    }

    Compositor::AeroWindow* window = wm->windowById(id);
    if (window != nullptr) {
        if (!view->windowTitle().isEmpty()) {
            window->setTitle(view->windowTitle());
        }
        if (!view->subtitle().isEmpty()) {
            window->setSubtitle(view->subtitle());
        }
    }

    connect(view, &Applications::AppView::titleChanged, this, [this, id](const QString& title) {
        if (m_desktop == nullptr || m_desktop->windowManager() == nullptr) {
            return;
        }
        if (Compositor::AeroWindow* target = m_desktop->windowManager()->windowById(id)) {
            target->setTitle(title);
        }
    });
    connect(view, &Applications::AppView::subtitleChanged, this, [this, id](const QString& subtitle) {
        if (m_desktop == nullptr || m_desktop->windowManager() == nullptr) {
            return;
        }
        if (Compositor::AeroWindow* target = m_desktop->windowManager()->windowById(id)) {
            target->setSubtitle(subtitle);
        }
    });
    connect(view, &Applications::AppView::closeRequested, this, [this, id]() {
        if (m_desktop != nullptr && m_desktop->windowManager() != nullptr) {
            m_desktop->windowManager()->closeWindow(id);
        }
    });
    connect(view, &Applications::AppView::statusMessage, this,
            [this, appId, name = definition->name](const QString& message) {
                if (m_notifications != nullptr && !message.isEmpty()) {
                    m_notifications->post(appId, name, name, message);
                }
            });

    return id;
}

void Shell::openPath(const QString& path)
{
    if (path.isEmpty()) {
        return;
    }

    const QFileInfo info(path);
    if (info.isDir()) {
        launchApplication(Applications::AppIds::Finder, info.absoluteFilePath());
        return;
    }
    if (!info.exists()) {
        if (m_notifications != nullptr) {
            m_notifications->post(QStringLiteral("aero.desktop"),
                                  QStringLiteral("Aero Desktop"),
                                  QStringLiteral("Path not found"),
                                  path);
        }
        return;
    }

    const QString suffix = info.suffix().toLower();
    if (matchesSuffix(suffix, archiveSuffixes())) {
        launchApplication(Applications::AppIds::Archive, info.absoluteFilePath());
    } else if (matchesSuffix(suffix, textSuffixes())) {
        launchApplication(Applications::AppIds::Editor, info.absoluteFilePath());
    } else {
        launchApplication(Applications::AppIds::Finder, info.absoluteFilePath());
    }
}

void Shell::hideAllOverlaysExcept(QWidget* exception)
{
    if (m_desktop != nullptr) {
        m_desktop->hideOverlays(exception);
    }
}

void Shell::toggleSpotlight()
{
    if (m_spotlight == nullptr || m_desktop == nullptr) {
        return;
    }
    if (m_spotlight->isVisible()) {
        m_spotlight->dismiss();
        return;
    }
    m_desktop->showOverlay(m_spotlight);
}

void Shell::toggleControlCenter()
{
    if (m_controlCenter == nullptr || m_desktop == nullptr) {
        return;
    }
    if (m_controlCenter->isVisible()) {
        m_controlCenter->dismiss();
        return;
    }
    m_desktop->showOverlay(m_controlCenter);
}

void Shell::toggleNotificationCenter()
{
    if (m_notificationCenter == nullptr || m_desktop == nullptr) {
        return;
    }
    if (m_notificationCenter->isVisible()) {
        m_notificationCenter->dismiss();
        return;
    }
    m_desktop->showOverlay(m_notificationCenter);
}

void Shell::toggleCalendarPopup()
{
    if (m_calendarPopup == nullptr || m_desktop == nullptr) {
        return;
    }
    if (m_calendarPopup->isVisible()) {
        m_calendarPopup->dismiss();
        return;
    }
    m_desktop->showOverlay(m_calendarPopup);
}

void Shell::toggleWorkspaceOverview()
{
    if (m_workspaceOverview == nullptr || m_desktop == nullptr) {
        return;
    }
    if (m_workspaceOverview->isVisible()) {
        m_workspaceOverview->dismiss();
        return;
    }
    m_desktop->showOverlay(m_workspaceOverview);
}

void Shell::showAppSwitcher(bool reverse)
{
    if (m_appSwitcher == nullptr || m_desktop == nullptr) {
        return;
    }
    hideAllOverlaysExcept(m_appSwitcher);
    m_appSwitcher->begin(reverse ? -1 : 1);
    m_switcherOpen = m_appSwitcher->isVisible();
}

void Shell::cycleAppSwitcher(bool reverse)
{
    if (m_appSwitcher == nullptr || !m_appSwitcher->isVisible()) {
        return;
    }
    m_appSwitcher->cycle(reverse ? -1 : 1);
}

void Shell::commitAppSwitcher()
{
    m_switcherOpen = false;
    if (m_appSwitcher != nullptr && m_appSwitcher->isVisible()) {
        m_appSwitcher->commit();
    }
}

void Shell::cancelAppSwitcher()
{
    m_switcherOpen = false;
    if (m_appSwitcher != nullptr && m_appSwitcher->isVisible()) {
        m_appSwitcher->cancel();
    }
}

void Shell::lockScreen()
{
    bool locked = false;
    if (m_platform != nullptr) {
        if (Platform::ISessionManager* session = m_platform->sessionManager()) {
            locked = session->isAvailable() && session->lockSession();
        }
        if (!locked) {
            if (Platform::IPowerManager* power = m_platform->powerManager()) {
                locked = power->isAvailable() && power->capabilities().canLock && power->lock();
            }
        }
    }

    if (!locked && m_notifications != nullptr) {
        m_notifications->post(QStringLiteral("aero.desktop"),
                              QStringLiteral("Aero Desktop"),
                              QStringLiteral("Lock unavailable"),
                              QStringLiteral("This backend does not expose a working session lock. "
                                             "Aero never shows a fake lock screen."));
    }
}

void Shell::requestLogout()
{
    if (!confirm(QStringLiteral("Log Out"),
                 QStringLiteral("Log out of the current Aero session? Unsaved work may be lost."))) {
        return;
    }
    bool ok = false;
    if (m_platform != nullptr) {
        if (Platform::ISessionManager* session = m_platform->sessionManager()) {
            ok = session->isAvailable() && session->logoutSession();
        }
    }
    if (!ok && m_notifications != nullptr) {
        m_notifications->post(QStringLiteral("aero.desktop"),
                              QStringLiteral("Aero Desktop"),
                              QStringLiteral("Log out unavailable"),
                              QStringLiteral("The host session manager refused or does not support logging out."));
    }
}

void Shell::requestRestart()
{
    if (!confirm(QStringLiteral("Restart"),
                 QStringLiteral("Restart the computer? Unsaved work may be lost."))) {
        return;
    }
    bool ok = false;
    if (m_platform != nullptr) {
        if (Platform::IPowerManager* power = m_platform->powerManager()) {
            ok = power->isAvailable() && power->capabilities().canReboot && power->reboot();
        }
    }
    if (!ok && m_notifications != nullptr) {
        m_notifications->post(QStringLiteral("aero.desktop"),
                              QStringLiteral("Aero Desktop"),
                              QStringLiteral("Restart unavailable"),
                              QStringLiteral("The platform power service refused the request."));
    }
}

void Shell::requestPowerOff()
{
    if (!confirm(QStringLiteral("Shut Down"),
                 QStringLiteral("Shut down the computer? Unsaved work may be lost."))) {
        return;
    }
    bool ok = false;
    if (m_platform != nullptr) {
        if (Platform::IPowerManager* power = m_platform->powerManager()) {
            ok = power->isAvailable() && power->capabilities().canPowerOff && power->powerOff();
        }
    }
    if (!ok && m_notifications != nullptr) {
        m_notifications->post(QStringLiteral("aero.desktop"),
                              QStringLiteral("Aero Desktop"),
                              QStringLiteral("Shut down unavailable"),
                              QStringLiteral("The platform power service refused the request."));
    }
}

void Shell::showAbout()
{
    launchApplication(Applications::AppIds::About);
}

void Shell::handleLaunchRequest(const QString& appId, const QString& argument)
{
    if (!appId.isEmpty()) {
        launchApplication(appId, argument);
    }
}

void Shell::handleOpenRequest(const QString& path)
{
    openPath(path);
}

void Shell::openSettingsPage(const QString& page)
{
    launchApplication(Applications::AppIds::Settings, page);
}

void Shell::showDockAppMenu(const QString& appId, const QPoint& globalPosition)
{
    if (m_desktop == nullptr || m_desktop->windowManager() == nullptr) {
        return;
    }
    const Applications::AppDefinition* definition =
        Applications::AppRegistry::instance().find(appId);
    const QString name = definition != nullptr ? definition->name : appId;

    Compositor::WindowManager* wm = m_desktop->windowManager();
    const QList<Compositor::AeroWindow*> windows = wm->windowsForApplication(appId);

    QMenu menu;
    QAction* open = menu.addAction(appIconFor(appId), windows.isEmpty()
                                                          ? QStringLiteral("Open")
                                                          : QStringLiteral("Activate"));
    QAction* showAll = nullptr;
    if (!windows.isEmpty()) {
        showAll = menu.addAction(Theme::IconFactory::icon(QStringLiteral("windows"), 16),
                                 QStringLiteral("Show All Windows"));
    }
    menu.addSeparator();
    QAction* settings = menu.addAction(Theme::IconFactory::icon(QStringLiteral("gear"), 16),
                                       QStringLiteral("Dock Settings…"));
    QAction* remove = menu.addAction(Theme::IconFactory::icon(QStringLiteral("minus"), 16),
                                     QStringLiteral("Remove from Dock"));
    remove->setEnabled(definition != nullptr);
    QAction* closeAll = nullptr;
    if (!windows.isEmpty()) {
        menu.addSeparator();
        closeAll = menu.addAction(Theme::IconFactory::icon(QStringLiteral("close"), 16),
                                  QStringLiteral("Quit %1").arg(name));
    }

    QAction* chosen = menu.exec(globalPosition);
    if (chosen == nullptr) {
        return;
    }
    if (chosen == open) {
        if (windows.isEmpty()) {
            launchApplication(appId);
            if (Dock* dock = m_desktop->dock()) {
                dock->bounce(appId);
            }
        } else {
            wm->restoreWindow(windows.last()->id());
        }
    } else if (chosen == showAll) {
        const int workspace = windows.last()->workspace();
        wm->switchWorkspace(workspace);
        for (Compositor::AeroWindow* window : windows) {
            if (window != nullptr && window->state() == Compositor::AeroWindow::State::Minimized) {
                wm->restoreWindow(window->id());
            }
        }
        if (!windows.isEmpty() && windows.last() != nullptr) {
            wm->activateWindow(windows.last()->id());
        }
    } else if (chosen == settings) {
        openSettingsPage(QStringLiteral("Dock"));
    } else if (chosen == remove) {
        if (m_settings != nullptr) {
            QStringList pinned = m_settings->pinnedApplications();
            pinned.removeAll(appId);
            m_settings->setPinnedApplications(pinned);
        }
    } else if (chosen == closeAll) {
        wm->closeAllForApplication(appId);
    }
}

void Shell::showDockMenu(const QPoint& globalPosition)
{
    QMenu menu;
    QAction* spotlight = menu.addAction(Theme::IconFactory::icon(QStringLiteral("search"), 16),
                                        QStringLiteral("Application Launcher…"));
    QAction* overview = menu.addAction(Theme::IconFactory::icon(QStringLiteral("grid"), 16),
                                       QStringLiteral("Workspace Overview"));
    menu.addSeparator();
    QAction* wallpaper = menu.addAction(Theme::IconFactory::icon(QStringLiteral("pictures"), 16),
                                        QStringLiteral("Change Wallpaper…"));
    QAction* dockSettings = menu.addAction(Theme::IconFactory::icon(QStringLiteral("dock"), 16),
                                           QStringLiteral("Dock Settings…"));
    QAction* systemSettings = menu.addAction(Theme::IconFactory::icon(QStringLiteral("gear"), 16),
                                             QStringLiteral("System Settings…"));

    QAction* chosen = menu.exec(globalPosition);
    if (chosen == spotlight) {
        toggleSpotlight();
    } else if (chosen == overview) {
        toggleWorkspaceOverview();
    } else if (chosen == wallpaper) {
        openSettingsPage(QStringLiteral("Wallpaper"));
    } else if (chosen == dockSettings) {
        openSettingsPage(QStringLiteral("Dock"));
    } else if (chosen == systemSettings) {
        openSettingsPage();
    }
}

bool Shell::confirm(const QString& title, const QString& text)
{
    const QMessageBox::StandardButton answer =
        QMessageBox::question(nullptr, title, text,
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    return answer == QMessageBox::Yes;
}

} // namespace Aero::Shell
