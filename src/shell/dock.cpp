#include "aero/shell/dock.h"

#include "aero/apps/app_registry.h"
#include "aero/compositor/window_manager.h"
#include "aero/core/utils.h"
#include "aero/settings/settings_manager.h"
#include "aero/theme/icon_factory.h"
#include "aero/theme/theme.h"

#include <QContextMenuEvent>
#include <QEnterEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QRadialGradient>
#include <QSet>
#include <QTimer>
#include <QToolTip>

#include <cmath>
#include <utility>

namespace Aero::Shell {
namespace {
constexpr int kShadowMargin = 14;
constexpr int kPadding = 10;
constexpr int kGap = 5;
constexpr int kBounceDurationMs = 760;
} // namespace

Dock::Dock(Settings::SettingsManager* settings,
           Compositor::WindowManager* windowManager,
           QWidget* parent)
    : Graphics::GlassPanel(parent)
    , m_settings(settings)
    , m_windowManager(windowManager)
{
    setCornerRadius(22);
    setShadowMargin(kShadowMargin);
    setTintStrength(1.0);
    setMouseTracking(true);
    setFocusPolicy(Qt::NoFocus);
    setAttribute(Qt::WA_Hover, true);

    m_animationTimer = new QTimer(this);
    m_animationTimer->setInterval(16);
    connect(m_animationTimer, &QTimer::timeout, this, &Dock::updateBounce);

    if (m_windowManager != nullptr) {
        connect(m_windowManager, &Compositor::WindowManager::windowListChanged,
                this, &Dock::refresh);
        connect(m_windowManager, &Compositor::WindowManager::activeWindowChanged,
                this, [this](int) { update(); });
    }

    rebuildItems();
}

int Dock::iconSize() const
{
    return m_settings != nullptr ? m_settings->dockIconSize() : 48;
}

int Dock::itemGap() const
{
    return kGap;
}

int Dock::surfacePadding() const
{
    return kPadding;
}

int Dock::preferredThickness() const
{
    return iconSize() + 2 * surfacePadding() + 2 * kShadowMargin;
}

QSize Dock::preferredSize() const
{
    const int count = qMax(1, m_apps.size());
    const int cell = iconSize() + 10;
    const int content = count * cell + qMax(0, count - 1) * itemGap();
    const int horizontal = content + 2 * surfacePadding() + 2 * kShadowMargin;
    const int vertical = preferredThickness();
    return m_orientation == Qt::Horizontal ? QSize(horizontal, vertical)
                                           : QSize(vertical, horizontal);
}

QSize Dock::sizeHint() const
{
    return preferredSize();
}

QSize Dock::minimumSizeHint() const
{
    return preferredSize();
}

void Dock::setOrientation(Qt::Orientation orientation)
{
    if (m_orientation == orientation) {
        return;
    }
    m_orientation = orientation;
    updateGeometry();
    update();
    emit preferredSizeChanged();
}

Qt::Orientation Dock::orientation() const
{
    return m_orientation;
}

void Dock::setConcealed(bool concealed)
{
    if (m_concealed == concealed) {
        return;
    }
    m_concealed = concealed;
    if (concealed) {
        m_inside = false;
        m_hovered = -1;
        m_pointer = {};
        QToolTip::hideText();
    }
}

bool Dock::isConcealed() const
{
    return m_concealed;
}

void Dock::bounce(const QString& appId)
{
    if (appId.isEmpty()) {
        return;
    }
    m_bounceApp = appId;
    m_bounceTimer.start();
    m_animationTicks = 0;
    startAnimation();
    update();
}

void Dock::refresh()
{
    const int oldWidth = preferredSize().width();
    const int oldHeight = preferredSize().height();
    rebuildItems();
    const QSize next = preferredSize();
    if (next.width() != oldWidth || next.height() != oldHeight) {
        updateGeometry();
        emit preferredSizeChanged();
    }
    update();
}

void Dock::rebuildItems()
{
    QStringList apps;
    if (m_settings != nullptr) {
        apps = m_settings->pinnedApplications();
    } else {
        apps = Applications::AppRegistry::instance().ids();
    }

    QSet<QString> running;
    if (m_windowManager != nullptr) {
        for (Compositor::AeroWindow* window : m_windowManager->windows()) {
            if (window != nullptr) {
                running.insert(window->applicationId());
            }
        }
    }

    const QStringList runningOnly = [&running, &apps]() {
        QStringList extra;
        for (const QString& appId : running) {
            if (!apps.contains(appId)) {
                extra.append(appId);
            }
        }
        extra.sort();
        return extra;
    }();

    apps.append(runningOnly);

    m_apps = apps;
    m_running.clear();
    m_running.reserve(apps.size());
    for (const QString& appId : std::as_const(m_apps)) {
        m_running.append(running.contains(appId));
    }

    if (m_hovered >= m_apps.size()) {
        m_hovered = -1;
    }
}

QRectF Dock::itemRect(int index) const
{
    if (index < 0 || index >= m_apps.size()) {
        return {};
    }
    const QRectF surface = surfaceRect();
    const int cell = iconSize() + 10;
    const int count = m_apps.size();
    const qreal content = count * cell + qMax(0, count - 1) * itemGap();

    if (m_orientation == Qt::Horizontal) {
        const qreal startX = surface.center().x() - content / 2.0;
        return QRectF(startX + index * (cell + itemGap()),
                      surface.center().y() - cell / 2.0,
                      cell,
                      cell);
    }

    const qreal startY = surface.center().y() - content / 2.0;
    return QRectF(surface.center().x() - cell / 2.0,
                  startY + index * (cell + itemGap()),
                  cell,
                  cell);
}

QRectF Dock::iconRect(int index) const
{
    const QRectF cell = itemRect(index);
    if (cell.isEmpty()) {
        return {};
    }
    const qreal size = iconSize();
    return QRectF(cell.center().x() - size / 2.0,
                  cell.center().y() - size / 2.0,
                  size,
                  size);
}

int Dock::indexAt(const QPointF& position) const
{
    for (int i = 0; i < m_apps.size(); ++i) {
        if (itemRect(i).adjusted(-itemGap() / 2.0, -itemGap() / 2.0,
                                itemGap() / 2.0, itemGap() / 2.0)
                .contains(position)) {
            return i;
        }
    }
    return -1;
}

void Dock::paintEvent(QPaintEvent* event)
{
    Graphics::GlassPanel::paintEvent(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const Theme::ThemeTokens& tokens = Theme::ThemeManager::instance().tokens();
    const qreal magnification = m_settings != nullptr ? m_settings->dockMagnification() : 1.3;
    const int activeAppWindow = m_windowManager != nullptr ? m_windowManager->activeWindowId() : -1;
    const QString activeApp = activeAppWindow >= 0 && m_windowManager != nullptr
        ? (m_windowManager->windowById(activeAppWindow) != nullptr
               ? m_windowManager->windowById(activeAppWindow)->applicationId()
               : QString())
        : QString();

    for (int i = 0; i < m_apps.size(); ++i) {
        const QString appId = m_apps.at(i);
        const QRectF cell = itemRect(i);
        const QRectF base = iconRect(i);
        if (cell.isEmpty() || base.isEmpty()) {
            continue;
        }

        qreal scale = 1.0;
        if (m_inside && !m_pointer.isNull()) {
            const QPointF center = base.center();
            const qreal distance = m_orientation == Qt::Horizontal
                ? qAbs(center.x() - m_pointer.x())
                : qAbs(center.y() - m_pointer.y());
            const qreal reach = iconSize() * 1.65;
            if (distance < reach) {
                const qreal influence = 1.0 - distance / reach;
                scale += (magnification - 1.0) * influence * influence;
            }
        }

        qreal bounceOffset = 0.0;
        if (!m_bounceApp.isEmpty() && m_bounceApp == appId) {
            const qreal t = Core::Utils::clamp(
                m_bounceTimer.elapsed() / static_cast<qreal>(kBounceDurationMs), 0.0, 1.0);
            bounceOffset = -qAbs(std::sin(t * 3.141592653589793 * 3.0)) * (1.0 - t) * 12.0;
        }

        const qreal size = iconSize() * scale;
        QRectF destination(cell.center().x() - size / 2.0,
                           cell.center().y() - size / 2.0 + bounceOffset,
                           size,
                           size);
        if (m_orientation == Qt::Horizontal) {
            destination.moveBottom(base.bottom() + bounceOffset);
        } else {
            destination.moveCenter(base.center() + QPointF(0, bounceOffset));
        }

        if (i == m_hovered) {
            const qreal haloSize = size + 12.0;
            QRadialGradient halo(destination.center(), haloSize / 2.0);
            QColor glow = tokens.color.accent;
            glow.setAlphaF(0.20);
            halo.setColorAt(0.0, glow);
            halo.setColorAt(1.0, QColor(glow.red(), glow.green(), glow.blue(), 0));
            painter.setPen(Qt::NoPen);
            painter.setBrush(halo);
            painter.drawEllipse(destination.center(), haloSize / 2.0, haloSize / 2.0);
        }

        const QIcon icon = Theme::IconFactory::appIcon(appId, qMax(32, qRound(iconSize() * 1.5)));
        icon.paint(&painter, destination.toRect(), Qt::AlignCenter, QIcon::Normal, QIcon::Off);

        if (m_running.value(i, false)) {
            QColor indicator = (appId == activeApp) ? tokens.color.accent : tokens.color.textSecondary;
            if (appId == activeApp) {
                indicator.setAlphaF(0.95);
            } else {
                indicator.setAlphaF(0.55);
            }
            painter.setPen(Qt::NoPen);
            painter.setBrush(indicator);
            const qreal dotRadius = appId == activeApp ? 2.4 : 1.9;
            if (m_orientation == Qt::Horizontal) {
                painter.drawEllipse(QPointF(cell.center().x(), surfaceRect().bottom() - 5.0),
                                    dotRadius, dotRadius);
            } else {
                painter.drawEllipse(QPointF(surfaceRect().right() - 5.0, cell.center().y()),
                                    dotRadius, dotRadius);
            }
        }
    }
}

void Dock::resizeEvent(QResizeEvent* event)
{
    Graphics::GlassPanel::resizeEvent(event);
    update();
}

void Dock::mouseMoveEvent(QMouseEvent* event)
{
    m_inside = true;
    m_pointer = event->position();
    const int index = indexAt(m_pointer);
    if (index != m_hovered) {
        m_hovered = index;
        if (index >= 0) {
            const QString appId = m_apps.at(index);
            const Applications::AppDefinition* definition =
                Applications::AppRegistry::instance().find(appId);
            const QString name = definition != nullptr ? definition->name : appId;
            QToolTip::showText(event->globalPosition().toPoint(), name, this,
                               itemRect(index).toRect());
        } else {
            QToolTip::hideText();
        }
    }
    update();
    Graphics::GlassPanel::mouseMoveEvent(event);
}

void Dock::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        const int index = indexAt(event->position());
        if (index >= 0) {
            event->accept();
            emit appActivated(m_apps.at(index));
            return;
        }
    }
    Graphics::GlassPanel::mousePressEvent(event);
}

void Dock::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        const int index = indexAt(event->position());
        if (index >= 0) {
            event->accept();
            emit appActivated(m_apps.at(index));
            return;
        }
    }
    Graphics::GlassPanel::mouseDoubleClickEvent(event);
}

void Dock::contextMenuEvent(QContextMenuEvent* event)
{
    const int index = indexAt(event->pos());
    if (index >= 0) {
        emit appMenuRequested(m_apps.at(index), event->globalPos());
    } else {
        emit dockMenuRequested(event->globalPos());
    }
    event->accept();
}

void Dock::enterEvent(QEnterEvent* event)
{
    m_inside = true;
    update();
    Graphics::GlassPanel::enterEvent(event);
}

void Dock::leaveEvent(QEvent* event)
{
    m_inside = false;
    m_hovered = -1;
    m_pointer = {};
    QToolTip::hideText();
    update();
    Graphics::GlassPanel::leaveEvent(event);
}

void Dock::requestRepaint()
{
    update();
}

void Dock::startAnimation()
{
    if (m_animationTimer != nullptr && !m_animationTimer->isActive()) {
        m_animationTimer->start();
    }
}

void Dock::stopAnimation()
{
    if (m_animationTimer != nullptr) {
        m_animationTimer->stop();
    }
}

void Dock::updateBounce()
{
    ++m_animationTicks;
    if (m_animationTicks > 2 && m_bounceTimer.elapsed() >= kBounceDurationMs) {
        m_bounceApp.clear();
        stopAnimation();
    }
    update();
}

} // namespace Aero::Shell
