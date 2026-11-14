#include "aero/shell/notification_center.h"

#include "aero/core/utils.h"
#include "aero/settings/settings_keys.h"
#include "aero/settings/settings_manager.h"
#include "aero/theme/controls.h"
#include "aero/theme/icon_factory.h"
#include "aero/theme/style.h"
#include "aero/theme/theme.h"

#include <QAbstractButton>
#include <QEnterEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>

#include <utility>

namespace Aero::Shell {
namespace {

void clearLayout(QLayout* layout)
{
    if (layout == nullptr) {
        return;
    }
    while (QLayoutItem* item = layout->takeAt(0)) {
        if (QWidget* widget = item->widget()) {
            widget->deleteLater();
        } else if (QLayout* child = item->layout()) {
            clearLayout(child);
            child->deleteLater();
        }
        delete item;
    }
}

QLabel* makeLabel(const QString& text, const QString& role, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    if (!role.isEmpty()) {
        label->setProperty("role", role);
    }
    return label;
}

} // namespace

NotificationCenter::NotificationCenter(Services::NotificationBus* bus,
                                       Settings::SettingsManager* settings,
                                       QWidget* parent)
    : Overlay(parent)
    , m_bus(bus)
    , m_settings(settings)
{
    setCornerRadius(20);
    setShadowMargin(18);
    setTintStrength(1.12);
    resize(400, 580);
    build();

    if (m_bus != nullptr) {
        connect(m_bus, &Services::NotificationBus::changed, this, &NotificationCenter::refresh);
    }
    if (m_settings != nullptr) {
        connect(m_settings, &Settings::SettingsManager::settingChanged, this,
                [this](const QString& key, const QVariant&) {
                    if (key == QString::fromLatin1(Settings::Keys::NotificationsDnd)) {
                        refresh();
                    }
                });
    }
    refresh();
}

void NotificationCenter::build()
{
    const int margin = shadowMargin();
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(margin + 16, margin + 14, margin + 16, margin + 14);
    root->setSpacing(10);

    auto* header = new QHBoxLayout;
    header->setSpacing(8);
    auto* title = new QLabel(QStringLiteral("Notifications"), this);
    title->setFont(Theme::ThemeManager::instance().font(19.0, QFont::DemiBold));
    header->addWidget(title);
    header->addStretch(1);

    auto* markRead = new Theme::IconButton(QStringLiteral("check"), this);
    markRead->setIconSize(14);
    markRead->setFixedSize(28, 28);
    markRead->setToolTip(QStringLiteral("Mark all as read"));
    connect(markRead, &QAbstractButton::clicked, this, [this]() {
        if (m_bus != nullptr) {
            m_bus->markAllRead();
        }
    });
    header->addWidget(markRead);

    auto* close = new Theme::IconButton(QStringLiteral("close"), this);
    close->setIconSize(13);
    close->setFixedSize(28, 28);
    close->setToolTip(QStringLiteral("Close"));
    connect(close, &QAbstractButton::clicked, this, &NotificationCenter::dismiss);
    header->addWidget(close);
    root->addLayout(header);

    auto* dndCard = new Theme::Card(this);
    auto* dndLayout = new QHBoxLayout(dndCard);
    dndLayout->setContentsMargins(12, 8, 12, 8);
    dndLayout->setSpacing(10);
    auto* moon = new QLabel(dndCard);
    moon->setFixedSize(20, 20);
    moon->setPixmap(Theme::IconFactory::pixmap(
        QStringLiteral("moon"), 18,
        Theme::ThemeManager::instance().tokens().color.textSecondary,
        dndCard->devicePixelRatioF()));
    dndLayout->addWidget(moon);
    auto* dndText = new QWidget(dndCard);
    auto* dndTextLayout = new QVBoxLayout(dndText);
    dndTextLayout->setContentsMargins(0, 0, 0, 0);
    dndTextLayout->setSpacing(0);
    auto* dndTitle = makeLabel(QStringLiteral("Do Not Disturb"), QStringLiteral("value"), dndText);
    auto* dndSub = makeLabel(QStringLiteral("Silence banners and sounds"), QStringLiteral("muted"), dndText);
    dndSub->setFont(Theme::ThemeManager::instance().captionFont());
    dndTextLayout->addWidget(dndTitle);
    dndTextLayout->addWidget(dndSub);
    dndLayout->addWidget(dndText, 1);
    m_dnd = new Theme::ToggleSwitch(dndCard);
    dndLayout->addWidget(m_dnd, 0, Qt::AlignVCenter);
    connect(m_dnd, &QAbstractButton::toggled, this, [this](bool enabled) {
        if (m_updating) {
            return;
        }
        if (m_bus != nullptr) {
            m_bus->setDoNotDisturb(enabled);
        } else if (m_settings != nullptr) {
            m_settings->setDoNotDisturb(enabled);
        }
    });
    root->addWidget(dndCard);

    m_summary = makeLabel(QStringLiteral("No notifications"), QStringLiteral("muted"), this);
    m_summary->setFont(Theme::ThemeManager::instance().captionFont());
    root->addWidget(m_summary);

    m_scroll = new QScrollArea(this);
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scroll->setStyleSheet(QStringLiteral("QScrollArea { background: transparent; border: none; }"));
    auto* content = new QWidget(m_scroll);
    content->setAttribute(Qt::WA_TranslucentBackground, true);
    content->setAutoFillBackground(false);
    m_cardsLayout = new QVBoxLayout(content);
    m_cardsLayout->setContentsMargins(0, 0, 0, 0);
    m_cardsLayout->setSpacing(8);
    m_cardsLayout->addStretch(1);
    m_scroll->setWidget(content);
    root->addWidget(m_scroll, 1);

    m_emptyLabel = makeLabel(QStringLiteral("You're all caught up.\nNotifications you receive will appear here."),
                             QStringLiteral("muted"), content);
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setWordWrap(true);
    m_emptyLabel->setMinimumHeight(150);
    m_cardsLayout->insertWidget(0, m_emptyLabel);

    auto* footer = new QHBoxLayout;
    footer->setSpacing(8);
    auto* settings = new Theme::AeroButton(QStringLiteral("Notification Settings"), QStringLiteral("gear"), this);
    settings->setVariant(Theme::AeroButton::Variant::Ghost);
    connect(settings, &QAbstractButton::clicked, this, [this]() {
        emit settingsRequested();
        dismiss();
    });
    auto* clear = new Theme::AeroButton(QStringLiteral("Clear All"), QStringLiteral("trash"), this);
    clear->setVariant(Theme::AeroButton::Variant::Ghost);
    connect(clear, &QAbstractButton::clicked, this, [this]() {
        if (m_bus != nullptr) {
            m_bus->clearAll();
        }
    });
    footer->addWidget(settings);
    footer->addStretch(1);
    footer->addWidget(clear);
    root->addLayout(footer);
}

void NotificationCenter::refresh()
{
    if (m_summary == nullptr) {
        return;
    }

    const QList<Services::Notification> notifications =
        m_bus != nullptr ? m_bus->notifications() : QList<Services::Notification>();
    const int unread = m_bus != nullptr ? m_bus->unreadCount() : 0;
    m_summary->setText(notifications.isEmpty()
                           ? QStringLiteral("No notifications")
                           : QStringLiteral("%1 total · %2 unread")
                                 .arg(notifications.size())
                                 .arg(unread));

    if (m_dnd != nullptr) {
        m_updating = true;
        m_dnd->setChecked(m_bus != nullptr
                              ? m_bus->doNotDisturb()
                              : (m_settings != nullptr && m_settings->doNotDisturb()));
        m_updating = false;
    }

    rebuildCards();
}

void NotificationCenter::rebuildCards()
{
    if (m_cardsLayout == nullptr) {
        return;
    }

    // Keep the placeholder label alive; everything else in the layout is a
    // generated card and is safe to delete.
    while (m_cardsLayout->count() > 0) {
        QLayoutItem* item = m_cardsLayout->takeAt(0);
        if (item->widget() != nullptr && item->widget() != m_emptyLabel) {
            item->widget()->deleteLater();
        }
        delete item;
    }

    const QList<Services::Notification> notifications =
        m_bus != nullptr ? m_bus->notifications() : QList<Services::Notification>();

    if (notifications.isEmpty()) {
        m_emptyLabel->show();
        m_cardsLayout->addWidget(m_emptyLabel);
        m_cardsLayout->addStretch(1);
        return;
    }

    m_emptyLabel->hide();

    for (const Services::Notification& notification : notifications) {
        auto* card = new Theme::Card(m_scroll->widget());
        auto* cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(12, 10, 10, 10);
        cardLayout->setSpacing(5);

        auto* top = new QHBoxLayout;
        top->setSpacing(8);
        auto* icon = new QLabel(card);
        icon->setFixedSize(24, 24);
        const QIcon glyph = notification.appId.isEmpty()
            ? Theme::IconFactory::icon(QStringLiteral("bell"), 24)
            : Theme::IconFactory::appIcon(notification.appId, 24);
        icon->setPixmap(glyph.pixmap(24, 24));
        top->addWidget(icon);

        auto* app = new QLabel(notification.appName, card);
        app->setFont(Theme::ThemeManager::instance().captionFont(QFont::DemiBold));
        app->setProperty("role", QStringLiteral("accent"));
        top->addWidget(app);
        top->addStretch(1);

        auto* time = new QLabel(Core::Utils::formatRelativeTime(notification.timestamp), card);
        time->setProperty("role", QStringLiteral("muted"));
        time->setFont(Theme::ThemeManager::instance().captionFont());
        top->addWidget(time);

        if (!notification.read) {
            auto* dot = new QLabel(card);
            dot->setFixedSize(7, 7);
            dot->setStyleSheet(QStringLiteral("QLabel { background: %1; border-radius: 3px; }")
                                   .arg(Theme::Style::cssColor(
                                       Theme::ThemeManager::instance().tokens().color.accent)));
            top->addWidget(dot, 0, Qt::AlignVCenter);
        }

        auto* dismissButton = new Theme::IconButton(QStringLiteral("close"), card);
        dismissButton->setIconSize(10);
        dismissButton->setFixedSize(22, 22);
        dismissButton->setToolTip(QStringLiteral("Dismiss"));
        connect(dismissButton, &QAbstractButton::clicked, this, [this, notification]() {
            if (m_bus != nullptr) {
                m_bus->dismiss(notification.id);
            }
        });
        top->addWidget(dismissButton);
        cardLayout->addLayout(top);

        auto* title = new QLabel(notification.title, card);
        title->setFont(Theme::ThemeManager::instance().bodyFont(QFont::DemiBold));
        title->setWordWrap(true);
        cardLayout->addWidget(title);

        if (!notification.body.isEmpty()) {
            auto* body = new QLabel(notification.body, card);
            body->setProperty("role", QStringLiteral("muted"));
            body->setFont(Theme::ThemeManager::instance().captionFont());
            body->setWordWrap(true);
            cardLayout->addWidget(body);
        }

        if (!notification.appId.isEmpty()) {
            auto* open = new Theme::AeroButton(QStringLiteral("Open"), card);
            open->setVariant(Theme::AeroButton::Variant::Ghost);
            connect(open, &QAbstractButton::clicked, this, [this, notification]() {
                if (m_bus != nullptr) {
                    m_bus->markRead(notification.id);
                }
                emit notificationActivated(notification.appId);
                dismiss();
            });
            cardLayout->addWidget(open, 0, Qt::AlignLeft);
        }

        m_cardsLayout->addWidget(card);
    }
    m_cardsLayout->addStretch(1);
}

void NotificationCenter::prepareShow()
{
    refresh();
}

void NotificationCenter::paintEvent(QPaintEvent* event)
{
    Graphics::GlassPanel::paintEvent(event);
}

} // namespace Aero::Shell

// ---------------------------------------------------------------------------
// Notification banner
// ---------------------------------------------------------------------------

namespace Aero::Shell {

NotificationBanner::NotificationBanner(QWidget* parent)
    : Graphics::GlassPanel(parent)
{
    setCornerRadius(16);
    setShadowMargin(14);
    setTintStrength(1.18);
    setCursor(Qt::PointingHandCursor);
    resize(380, 122);
    setMouseTracking(true);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(shadowMargin() + 14, shadowMargin() + 10,
                               shadowMargin() + 12, shadowMargin() + 10);
    layout->setSpacing(10);

    auto* icon = new QLabel(this);
    icon->setObjectName(QStringLiteral("bannerIcon"));
    icon->setFixedSize(32, 32);
    icon->setPixmap(Theme::IconFactory::icon(QStringLiteral("bell"), 30).pixmap(30, 30));
    layout->addWidget(icon, 0, Qt::AlignTop);

    auto* textHost = new QWidget(this);
    auto* textLayout = new QVBoxLayout(textHost);
    textLayout->setContentsMargins(0, 0, 0, 0);
    textLayout->setSpacing(1);
    textHost->setAttribute(Qt::WA_TransparentForMouseEvents, true);

    m_appLabel = new QLabel(QStringLiteral("Aero Desktop"), textHost);
    m_appLabel->setProperty("role", QStringLiteral("muted"));
    m_appLabel->setFont(Theme::ThemeManager::instance().captionFont(QFont::DemiBold));
    textLayout->addWidget(m_appLabel);

    m_titleLabel = new QLabel(textHost);
    m_titleLabel->setFont(Theme::ThemeManager::instance().bodyFont(QFont::DemiBold));
    m_titleLabel->setWordWrap(true);
    textLayout->addWidget(m_titleLabel);

    m_bodyLabel = new QLabel(textHost);
    m_bodyLabel->setProperty("role", QStringLiteral("muted"));
    m_bodyLabel->setFont(Theme::ThemeManager::instance().captionFont());
    m_bodyLabel->setWordWrap(true);
    textLayout->addWidget(m_bodyLabel);
    layout->addWidget(textHost, 1);

    auto* close = new Theme::IconButton(QStringLiteral("close"), this);
    close->setIconSize(10);
    close->setFixedSize(22, 22);
    close->setToolTip(QStringLiteral("Dismiss"));
    connect(close, &QAbstractButton::clicked, this, &NotificationBanner::hideBanner);
    layout->addWidget(close, 0, Qt::AlignTop);

    m_timer = new QTimer(this);
    m_timer->setSingleShot(true);
    connect(m_timer, &QTimer::timeout, this, &NotificationBanner::hideBanner);
    hide();
}

void NotificationBanner::showNotification(const Services::Notification& notification, bool showBody)
{
    m_notification = notification;
    m_appLabel->setText(notification.appName.isEmpty() ? QStringLiteral("Aero Desktop")
                                                       : notification.appName);
    m_titleLabel->setText(notification.title);
    m_bodyLabel->setText(showBody ? notification.body : QString());
    m_bodyLabel->setVisible(showBody && !notification.body.isEmpty());

    auto* icon = findChild<QLabel*>(QStringLiteral("bannerIcon"));
    if (icon != nullptr) {
        icon->setPixmap(notification.appId.isEmpty()
                            ? Theme::IconFactory::icon(QStringLiteral("bell"), 30).pixmap(30, 30)
                            : Theme::IconFactory::appIcon(notification.appId, 30).pixmap(30, 30));
    }

    show();
    raise();
    if (m_timer != nullptr) {
        m_timer->start(6000);
    }
}

void NotificationBanner::hideBanner()
{
    if (m_timer != nullptr) {
        m_timer->stop();
    }
    hide();
}

void NotificationBanner::updateTimer()
{
    if (m_timer == nullptr || !isVisible()) {
        return;
    }
    if (m_hovered) {
        m_timer->stop();
    } else {
        m_timer->start(3000);
    }
}

void NotificationBanner::enterEvent(QEnterEvent* event)
{
    m_hovered = true;
    updateTimer();
    Graphics::GlassPanel::enterEvent(event);
}

void NotificationBanner::leaveEvent(QEvent* event)
{
    m_hovered = false;
    updateTimer();
    Graphics::GlassPanel::leaveEvent(event);
}

void NotificationBanner::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        emit activated(m_notification.appId, m_notification.id);
        hideBanner();
        event->accept();
        return;
    }
    Graphics::GlassPanel::mousePressEvent(event);
}

void NotificationBanner::paintEvent(QPaintEvent* event)
{
    Graphics::GlassPanel::paintEvent(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);
    QColor accent = Theme::ThemeManager::instance().tokens().color.accent;
    accent.setAlphaF(0.85);
    painter.setBrush(accent);
    const QRectF surface = surfaceRect();
    painter.drawRoundedRect(QRectF(surface.left() + 1, surface.top() + 14,
                                   3, surface.height() - 28),
                            1.5, 1.5);
}

} // namespace Aero::Shell
