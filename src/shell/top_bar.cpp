#include "aero/shell/top_bar.h"

#include "aero/apps/app_registry.h"
#include "aero/compositor/window_manager.h"
#include "aero/compositor/workspace_model.h"
#include "aero/platform/i_audio_manager.h"
#include "aero/platform/i_display_manager.h"
#include "aero/platform/i_network_manager.h"
#include "aero/platform/i_power_manager.h"
#include "aero/platform/platform.h"
#include "aero/services/notification_bus.h"
#include "aero/settings/settings_manager.h"
#include "aero/theme/controls.h"
#include "aero/theme/icon_factory.h"
#include "aero/theme/style.h"
#include "aero/theme/theme.h"

#include <QAction>
#include <QAbstractButton>
#include <QDateTime>
#include <QEnterEvent>
#include <QFont>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QTimer>
#include <QToolTip>

#include <functional>
#include <utility>

namespace Aero::Shell {

class TopBar::WorkspaceDots final : public QWidget {
public:
    explicit WorkspaceDots(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setMouseTracking(true);
        setCursor(Qt::PointingHandCursor);
        setToolTip(QStringLiteral("Switch workspace"));
    }

    void setState(int count, int active)
    {
        m_count = qMax(1, count);
        m_active = qBound(0, active, m_count - 1);
        setMinimumWidth(16 + m_count * 14);
        setMaximumWidth(16 + m_count * 14);
        update();
        updateGeometry();
    }

    std::function<void(int)> onSelected;
    std::function<void()> onOverviewRequested;

    QSize sizeHint() const override
    {
        return QSize(16 + m_count * 14, 22);
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        Q_UNUSED(event);
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        const Theme::ThemeTokens& tokens = Theme::ThemeManager::instance().tokens();

        const qreal spacing = 14.0;
        const qreal total = m_count * spacing;
        qreal x = (width() - total) / 2.0 + spacing / 2.0;
        const qreal y = height() / 2.0;

        for (int i = 0; i < m_count; ++i) {
            const bool active = i == m_active;
            const bool hovered = i == m_hovered;
            const qreal radius = active ? 4.2 : 3.0;
            QColor color = active ? tokens.color.accent : tokens.color.textSecondary;
            color.setAlphaF(active ? 0.95 : (hovered ? 0.9 : 0.4));
            painter.setPen(Qt::NoPen);
            painter.setBrush(color);
            painter.drawEllipse(QPointF(x, y), radius, radius);
            x += spacing;
        }
    }

    void mousePressEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton) {
            const int index = indexAt(event->position().x());
            if (index >= 0 && onSelected) {
                onSelected(index);
            }
            event->accept();
            return;
        }
        QWidget::mousePressEvent(event);
    }

    void mouseDoubleClickEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton && onOverviewRequested) {
            onOverviewRequested();
            event->accept();
            return;
        }
        QWidget::mouseDoubleClickEvent(event);
    }

    void mouseMoveEvent(QMouseEvent* event) override
    {
        const int index = indexAt(event->position().x());
        if (index != m_hovered) {
            m_hovered = index;
            if (index >= 0) {
                QToolTip::showText(event->globalPosition().toPoint(),
                                   QStringLiteral("Desktop %1").arg(index + 1), this);
            } else {
                QToolTip::hideText();
            }
            update();
        }
        QWidget::mouseMoveEvent(event);
    }

    void leaveEvent(QEvent* event) override
    {
        m_hovered = -1;
        update();
        QWidget::leaveEvent(event);
    }

private:
    int indexAt(qreal x) const
    {
        const qreal spacing = 14.0;
        const qreal start = (width() - m_count * spacing) / 2.0;
        const int index = static_cast<int>((x - start) / spacing);
        return qBound(0, index, m_count - 1);
    }

    int m_count = 4;
    int m_active = 0;
    int m_hovered = -1;
};

class TopBar::StatusButton final : public QAbstractButton {
public:
    explicit StatusButton(QWidget* parent = nullptr)
        : QAbstractButton(parent)
    {
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::NoFocus);
        setFixedSize(28, 26);
        setMouseTracking(true);
    }

    void setIconName(const QString& name)
    {
        if (m_iconName == name) {
            return;
        }
        m_iconName = name;
        update();
    }

    QString iconName() const
    {
        return m_iconName;
    }

    QSize sizeHint() const override
    {
        return QSize(28, 26);
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        Q_UNUSED(event);
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        const Theme::ThemeTokens& tokens = Theme::ThemeManager::instance().tokens();

        if (m_hovered || isDown()) {
            QColor hover = tokens.color.textPrimary;
            hover.setAlphaF(isDown() ? 0.14 : 0.08);
            painter.setPen(Qt::NoPen);
            painter.setBrush(hover);
            painter.drawRoundedRect(rect().adjusted(2, 2, -2, -2), 7, 7);
        }

        const QColor color = m_hovered ? tokens.color.textPrimary : tokens.color.textSecondary;
        const QPixmap pixmap = Theme::IconFactory::pixmap(m_iconName, 16, color,
                                                          devicePixelRatioF());
        const QRect target(0, 0, 16, 16);
        painter.drawPixmap(QRect((width() - 16) / 2, (height() - 16) / 2, 16, 16), pixmap);
        Q_UNUSED(target);
    }

    void enterEvent(QEnterEvent* event) override
    {
        m_hovered = true;
        update();
        QAbstractButton::enterEvent(event);
    }

    void leaveEvent(QEvent* event) override
    {
        m_hovered = false;
        update();
        QAbstractButton::leaveEvent(event);
    }

private:
    QString m_iconName = QStringLiteral("info");
    bool m_hovered = false;
};

TopBar::TopBar(Platform::Platform* platform,
               Settings::SettingsManager* settings,
               Services::NotificationBus* notifications,
               Compositor::WindowManager* windowManager,
               QWidget* parent)
    : Graphics::GlassPanel(parent)
    , m_platform(platform)
    , m_settings(settings)
    , m_notifications(notifications)
    , m_windowManager(windowManager)
{
    setCornerRadius(0);
    setShadowMargin(0);
    setTintStrength(1.05);
    setAttribute(Qt::WA_Hover, true);
    setFocusPolicy(Qt::NoFocus);
    setFixedHeight(40);
    build();
}

QSize TopBar::sizeHint() const
{
    return QSize(1440, 40);
}

void TopBar::build()
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(10, 4, 12, 4);
    layout->setSpacing(7);

    auto* menu = new Theme::IconButton(QStringLiteral("command"), this);
    menu->setIconSize(16);
    menu->setToolTip(QStringLiteral("Aero menu"));
    menu->setFixedSize(30, 28);
    m_menuButton = menu;
    connect(menu, &QAbstractButton::clicked, this, &TopBar::showSystemMenu);
    layout->addWidget(menu);

    m_appName = new QLabel(QStringLiteral("Aero Desktop"), this);
    m_appName->setFont(Theme::ThemeManager::instance().bodyFont(QFont::DemiBold));
    m_appName->setProperty("role", QStringLiteral("subtitle"));
    m_appName->setMinimumWidth(90);
    layout->addWidget(m_appName);

    m_workspaceDots = new WorkspaceDots(this);
    m_workspaceDots->onSelected = [this](int index) {
        if (m_windowManager != nullptr) {
            m_windowManager->switchWorkspace(index);
        }
    };
    m_workspaceDots->onOverviewRequested = [this]() { emit workspaceOverviewRequested(); };
    layout->addWidget(m_workspaceDots);

    layout->addStretch(1);

    auto* clock = new Theme::AeroButton(QString(), this);
    clock->setVariant(Theme::AeroButton::Variant::Ghost);
    clock->setFont(Theme::ThemeManager::instance().captionFont(QFont::Medium));
    clock->setToolTip(QStringLiteral("Open calendar"));
    m_clock = clock;
    connect(clock, &QAbstractButton::clicked, this, [this, clock]() {
        emit calendarRequested(clock->mapToGlobal(QPoint(clock->width() / 2, clock->height() + 4)));
    });
    layout->addWidget(clock);

    layout->addStretch(1);

    m_networkButton = new StatusButton(this);
    m_networkButton->setToolTip(QStringLiteral("Network"));
    connect(m_networkButton, &QAbstractButton::clicked, this, [this]() {
        emit controlCenterRequested(m_networkButton->mapToGlobal(QPoint(m_networkButton->width() / 2,
                                                                      m_networkButton->height() + 4)));
    });
    layout->addWidget(m_networkButton);

    m_volumeButton = new StatusButton(this);
    m_volumeButton->setToolTip(QStringLiteral("Sound"));
    connect(m_volumeButton, &QAbstractButton::clicked, this, [this]() {
        emit controlCenterRequested(m_volumeButton->mapToGlobal(QPoint(m_volumeButton->width() / 2,
                                                                     m_volumeButton->height() + 4)));
    });
    layout->addWidget(m_volumeButton);

    m_batteryButton = new StatusButton(this);
    m_batteryButton->setToolTip(QStringLiteral("Battery"));
    connect(m_batteryButton, &QAbstractButton::clicked, this, [this]() {
        emit controlCenterRequested(m_batteryButton->mapToGlobal(QPoint(m_batteryButton->width() / 2,
                                                                      m_batteryButton->height() + 4)));
    });
    layout->addWidget(m_batteryButton);

    auto* bell = new StatusButton(this);
    m_bellButton = bell;
    bell->setIconName(QStringLiteral("bell"));
    bell->setToolTip(QStringLiteral("Notifications"));
    connect(bell, &QAbstractButton::clicked, this, [this, bell]() {
        emit notificationCenterRequested(bell->mapToGlobal(QPoint(bell->width() / 2, bell->height() + 4)));
    });
    layout->addWidget(bell);

    m_badge = new QLabel(this);
    m_badge->setAlignment(Qt::AlignCenter);
    m_badge->setFixedSize(16, 16);
    m_badge->setFont(Theme::ThemeManager::instance().font(9.0, QFont::Bold));
    m_badge->setStyleSheet(QStringLiteral("QLabel { background-color: %1; color: white; "
                                          "border-radius: 8px; font-size: 9px; }")
                               .arg(Theme::Style::cssColor(Theme::ThemeManager::instance().tokens().color.danger)));
    m_badge->hide();

    auto* control = new Theme::IconButton(QStringLiteral("grid"), this);
    control->setIconSize(16);
    control->setToolTip(QStringLiteral("Control Center"));
    control->setFixedSize(30, 28);
    connect(control, &QAbstractButton::clicked, this, [this, control]() {
        emit controlCenterRequested(control->mapToGlobal(QPoint(control->width() / 2,
                                                                control->height() + 4)));
    });
    layout->addWidget(control);

    auto* search = new Theme::IconButton(QStringLiteral("search"), this);
    search->setIconSize(16);
    search->setToolTip(QStringLiteral("Spotlight (%1)")
                           .arg(m_settings != nullptr
                                    ? m_settings->shortcut(QStringLiteral("spotlight"))
                                          .toString(QKeySequence::NativeText)
                                    : QStringLiteral("Meta+Space")));
    search->setFixedSize(30, 28);
    connect(search, &QAbstractButton::clicked, this, &TopBar::spotlightRequested);
    layout->addWidget(search);

    m_clockTimer = new QTimer(this);
    m_clockTimer->setInterval(1000);
    m_clockTimer->setTimerType(Qt::CoarseTimer);
    connect(m_clockTimer, &QTimer::timeout, this, &TopBar::updateClock);
    m_clockTimer->start();

    m_statusTimer = new QTimer(this);
    m_statusTimer->setInterval(30000);
    m_statusTimer->setTimerType(Qt::VeryCoarseTimer);
    connect(m_statusTimer, &QTimer::timeout, this, &TopBar::refreshStatus);
    m_statusTimer->start();

    if (m_windowManager != nullptr) {
        connect(m_windowManager, &Compositor::WindowManager::activeWindowChanged,
                this, [this](int) { refreshApplicationName(); });
        connect(m_windowManager, &Compositor::WindowManager::windowListChanged,
                this, [this]() {
                    refreshApplicationName();
                    refreshWorkspaces();
                });
        connect(m_windowManager, &Compositor::WindowManager::workspaceChanged,
                this, [this](int) { refreshWorkspaces(); });
        if (m_windowManager->workspaces() != nullptr) {
            connect(m_windowManager->workspaces(), &Compositor::WorkspaceModel::countChanged,
                    this, [this](int) { refreshWorkspaces(); });
            connect(m_windowManager->workspaces(), &Compositor::WorkspaceModel::activeIndexChanged,
                    this, [this](int) { refreshWorkspaces(); });
        }
    }

    if (m_notifications != nullptr) {
        connect(m_notifications, &Services::NotificationBus::unreadCountChanged,
                this, &TopBar::setUnreadCount);
        setUnreadCount(m_notifications->unreadCount());
    }

    if (m_platform != nullptr) {
        if (auto* network = m_platform->networkManager()) {
            connect(network, &Platform::INetworkManager::networkChanged,
                    this, [this](const Platform::NetworkInfo&) { refreshStatus(); });
            connect(network, &Platform::INetworkManager::wifiNetworksChanged,
                    this, [this](const QList<Platform::WifiNetwork>&) { refreshStatus(); });
        }
        if (auto* audio = m_platform->audioManager()) {
            connect(audio, &Platform::IAudioManager::stateChanged,
                    this, [this](const Platform::AudioInfo&) { refreshStatus(); });
        }
        if (auto* power = m_platform->powerManager()) {
            connect(power, &Platform::IPowerManager::batteryChanged,
                    this, [this](const Platform::BatteryInfo&) { refreshStatus(); });
            connect(power, &Platform::IPowerManager::capabilitiesChanged,
                    this, &TopBar::refreshStatus);
        }
    }

    connect(&Theme::ThemeManager::instance(), &Theme::ThemeManager::themeChanged,
            this, [this]() {
                if (m_appName != nullptr) {
                    m_appName->setFont(Theme::ThemeManager::instance().bodyFont(QFont::DemiBold));
                }
                if (m_clock != nullptr) {
                    m_clock->setFont(Theme::ThemeManager::instance().captionFont(QFont::Medium));
                }
                refreshStatus();
                update();
            });

    updateClock();
    refreshStatus();
    refreshApplicationName();
    refreshWorkspaces();
}

void TopBar::refresh()
{
    updateClock();
    refreshStatus();
    refreshApplicationName();
    refreshWorkspaces();
    update();
}

void TopBar::setUnreadCount(int count)
{
    m_unreadCount = qMax(0, count);
    if (m_badge == nullptr) {
        return;
    }
    if (m_unreadCount <= 0) {
        m_badge->hide();
        if (m_bellButton != nullptr) {
            m_bellButton->setToolTip(QStringLiteral("Notifications"));
        }
        return;
    }
    m_badge->setText(m_unreadCount > 9 ? QStringLiteral("9+") : QString::number(m_unreadCount));
    m_badge->show();
    m_badge->raise();
    if (m_bellButton != nullptr) {
        m_bellButton->setToolTip(QStringLiteral("%1 unread notification(s)").arg(m_unreadCount));
    }
    positionBadge();
}

void TopBar::updateClock()
{
    if (m_clock == nullptr) {
        return;
    }
    const QDateTime now = QDateTime::currentDateTime();
    const QString text = now.toString(QStringLiteral("ddd d MMM   HH:mm"));
    m_clock->setText(text);
    m_clock->setMinimumWidth(qMax(120, m_clock->fontMetrics().horizontalAdvance(text) + 16));
    positionBadge();
}

void TopBar::refreshStatus()
{
    const Theme::ThemeTokens& tokens = Theme::ThemeManager::instance().tokens();
    Q_UNUSED(tokens);

    if (m_platform != nullptr) {
        if (auto* network = m_platform->networkManager()) {
            if (!network->isAvailable()) {
                m_networkButton->setIconName(QStringLiteral("wifi-off"));
                m_networkButton->setToolTip(QStringLiteral("Network management unavailable"));
            } else {
                const Platform::NetworkInfo info = network->network();
                if (!info.wifiEnabled) {
                    m_networkButton->setIconName(QStringLiteral("wifi-off"));
                    m_networkButton->setToolTip(QStringLiteral("Wi-Fi is off"));
                } else if (info.connected) {
                    m_networkButton->setIconName(info.wifi ? QStringLiteral("wifi") : QStringLiteral("globe"));
                    m_networkButton->setToolTip(
                        info.wifi
                            ? QStringLiteral("%1 · %2%").arg(info.ssid).arg(info.signalStrength)
                            : QStringLiteral("Wired connection · %1").arg(info.ipAddress));
                } else {
                    m_networkButton->setIconName(QStringLiteral("wifi"));
                    m_networkButton->setToolTip(QStringLiteral("Wi-Fi on · not connected"));
                }
            }
        } else {
            m_networkButton->setIconName(QStringLiteral("wifi-off"));
            m_networkButton->setToolTip(QStringLiteral("No network service"));
        }

        if (auto* audio = m_platform->audioManager()) {
            if (!audio->isAvailable()) {
                m_volumeButton->setIconName(QStringLiteral("volume-muted"));
                m_volumeButton->setToolTip(QStringLiteral("Audio unavailable"));
            } else {
                const Platform::AudioInfo info = audio->state();
                m_volumeButton->setIconName(info.muted || info.volume <= 0
                                                ? QStringLiteral("volume-muted")
                                                : QStringLiteral("volume"));
                m_volumeButton->setToolTip(info.muted
                                               ? QStringLiteral("Muted")
                                               : QStringLiteral("Volume %1%").arg(info.volume));
            }
        } else {
            m_volumeButton->setIconName(QStringLiteral("volume-muted"));
            m_volumeButton->setToolTip(QStringLiteral("No audio service"));
        }

        if (auto* power = m_platform->powerManager()) {
            const Platform::BatteryInfo battery = power->battery();
            if (battery.present) {
                m_batteryButton->setIconName(battery.charging ? QStringLiteral("battery-charging")
                                                              : QStringLiteral("battery"));
                m_batteryButton->setToolTip(
                    battery.charging
                        ? QStringLiteral("Battery %1% · charging").arg(battery.percent)
                        : QStringLiteral("Battery %1%").arg(battery.percent));
            } else {
                m_batteryButton->setIconName(QStringLiteral("battery"));
                m_batteryButton->setToolTip(QStringLiteral("AC power · no battery"));
            }
        } else {
            m_batteryButton->setIconName(QStringLiteral("battery"));
            m_batteryButton->setToolTip(QStringLiteral("Power information unavailable"));
        }
    }

    if (m_bellButton != nullptr) {
        m_bellButton->setIconName(m_unreadCount > 0 ? QStringLiteral("bell")
                                                    : QStringLiteral("bell-off"));
    }
    update();
}

void TopBar::refreshApplicationName()
{
    QString name = QStringLiteral("Aero Desktop");
    if (m_windowManager != nullptr) {
        if (Compositor::AeroWindow* active = m_windowManager->activeWindow()) {
            const Applications::AppDefinition* definition =
                Applications::AppRegistry::instance().find(active->applicationId());
            name = definition != nullptr ? definition->name : active->title();
        }
    }
    if (m_appName != nullptr && m_appName->text() != name) {
        m_appName->setText(name);
    }
}

void TopBar::refreshWorkspaces()
{
    if (m_windowManager == nullptr || m_windowManager->workspaces() == nullptr
        || m_workspaceDots == nullptr) {
        return;
    }
    m_workspaceDots->setState(m_windowManager->workspaces()->count(),
                              m_windowManager->workspaces()->activeIndex());
}

void TopBar::showSystemMenu()
{
    QMenu menu(this);
    QAction* about = menu.addAction(Theme::IconFactory::icon(QStringLiteral("info"), 16),
                                    QStringLiteral("About Aero Desktop"));
    QAction* settings = menu.addAction(Theme::IconFactory::icon(QStringLiteral("gear"), 16),
                                       QStringLiteral("System Settings…"));
    menu.addSeparator();
    QAction* lock = menu.addAction(Theme::IconFactory::icon(QStringLiteral("lock"), 16),
                                   QStringLiteral("Lock Screen"));
    QAction* logout = menu.addAction(Theme::IconFactory::icon(QStringLiteral("power"), 16),
                                     QStringLiteral("Log Out…"));
    menu.addSeparator();
    QAction* restart = menu.addAction(Theme::IconFactory::icon(QStringLiteral("refresh"), 16),
                                      QStringLiteral("Restart…"));
    QAction* powerOff = menu.addAction(Theme::IconFactory::icon(QStringLiteral("power"), 16),
                                       QStringLiteral("Shut Down…"));

    const QPoint anchor = m_menuButton != nullptr
        ? m_menuButton->mapToGlobal(QPoint(0, m_menuButton->height() + 4))
        : mapToGlobal(QPoint(12, height()));
    QAction* chosen = menu.exec(anchor);
    if (chosen == nullptr) {
        return;
    }
    if (chosen == about) {
        emit aboutRequested();
    } else if (chosen == settings) {
        emit settingsRequested();
    } else if (chosen == lock) {
        emit lockRequested();
    } else if (chosen == logout) {
        emit logoutRequested();
    } else if (chosen == restart) {
        emit restartRequested();
    } else if (chosen == powerOff) {
        emit powerOffRequested();
    }
}

void TopBar::positionBadge()
{
    if (m_badge == nullptr || m_bellButton == nullptr) {
        return;
    }
    const QRect bell = m_bellButton->geometry();
    m_badge->move(bell.right() - 6, bell.top() - 1);
    if (m_badge->isVisible()) {
        m_badge->raise();
    }
}

void TopBar::resizeEvent(QResizeEvent* event)
{
    Graphics::GlassPanel::resizeEvent(event);
    positionBadge();
}

void TopBar::paintEvent(QPaintEvent* event)
{
    Graphics::GlassPanel::paintEvent(event);

    QPainter painter(this);
    const Theme::ThemeTokens& tokens = Theme::ThemeManager::instance().tokens();
    QColor line = tokens.color.separator;
    line.setAlphaF(0.55);
    painter.setPen(line);
    painter.drawLine(QPointF(0, height() - 0.5), QPointF(width(), height() - 0.5));
}

} // namespace Aero::Shell
