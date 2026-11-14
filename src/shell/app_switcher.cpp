#include "aero/shell/app_switcher.h"

#include "aero/apps/app_registry.h"
#include "aero/compositor/window.h"
#include "aero/compositor/window_manager.h"
#include "aero/compositor/workspace_model.h"
#include "aero/theme/icon_factory.h"
#include "aero/theme/theme.h"

#include <QFont>
#include <QFontMetrics>
#include <QPainter>
#include <QPixmap>
#include <QWidget>

#include <algorithm>

namespace Aero::Shell {
namespace {

QString applicationName(const QString& appId, const QString& fallback)
{
    const Applications::AppDefinition* definition =
        Applications::AppRegistry::instance().find(appId);
    if (definition != nullptr && !definition->name.isEmpty()) {
        return definition->name;
    }
    return fallback;
}

} // namespace

AppSwitcher::AppSwitcher(Compositor::WindowManager* windowManager, QWidget* parent)
    : Overlay(parent)
    , m_windowManager(windowManager)
{
    setCornerRadius(24);
    setShadowMargin(22);
    setTintStrength(1.16);
    setFocusPolicy(Qt::StrongFocus);
    resize(720, 330);
}

void AppSwitcher::begin(int direction)
{
    refreshWindows();
    if (m_windows.isEmpty()) {
        m_selected = 0;
        emit selectionCancelled();
        return;
    }

    const int count = m_windows.size();
    const int columns = qMin(5, count);
    const int rows = (count + columns - 1) / columns;
    const int width = qBound(460, 128 + columns * 188, 1140);
    const int height = 164 + rows * 138;
    resize(width, height);
    if (QWidget* host = parentWidget()) {
        move(qMax(0, (host->width() - width) / 2),
             qMax(0, (host->height() - height) / 2 - 40));
    }

    if (count > 1) {
        m_selected = direction >= 0 ? 1 : count - 1;
    } else {
        m_selected = 0;
    }
    m_committed = false;
    present();
    update();
}

void AppSwitcher::refreshWindows()
{
    m_windows.clear();
    if (m_windowManager == nullptr) {
        return;
    }

    const int workspace = m_windowManager->workspaces() != nullptr
        ? m_windowManager->workspaces()->activeIndex()
        : 0;

    Compositor::AeroWindow* active = m_windowManager->activeWindow();
    if (active != nullptr
        && active->workspace() == workspace
        && active->state() != Compositor::AeroWindow::State::Minimized) {
        m_windows.append(active);
    }

    const QList<Compositor::AeroWindow*> all = m_windowManager->windows();
    for (Compositor::AeroWindow* window : all) {
        if (window == nullptr || window == active || window->workspace() != workspace
            || window->state() == Compositor::AeroWindow::State::Minimized) {
            continue;
        }
        m_windows.append(window);
    }

    clampSelection();
}

void AppSwitcher::clampSelection()
{
    if (m_windows.isEmpty()) {
        m_selected = 0;
        return;
    }
    m_selected = qBound(0, m_selected, m_windows.size() - 1);
}

void AppSwitcher::cycle(int direction)
{
    if (m_windows.isEmpty() || !isVisible()) {
        return;
    }
    const int count = m_windows.size();
    const int step = direction < 0 ? -1 : 1;
    m_selected = (m_selected + step + count) % count;
    update();
}

int AppSwitcher::selectedWindowId() const
{
    if (m_selected < 0 || m_selected >= m_windows.size()) {
        return -1;
    }
    Compositor::AeroWindow* window = m_windows.at(m_selected);
    return window != nullptr ? window->id() : -1;
}

void AppSwitcher::commit()
{
    if (!isVisible()) {
        return;
    }
    const int id = selectedWindowId();
    m_committed = true;
    dismiss();
    if (id >= 0) {
        emit windowChosen(id);
    } else {
        emit selectionCancelled();
    }
}

void AppSwitcher::cancel()
{
    if (!isVisible()) {
        return;
    }
    m_committed = false;
    dismiss();
    emit selectionCancelled();
}

void AppSwitcher::paintEvent(QPaintEvent* event)
{
    Overlay::paintEvent(event);

    if (m_windows.isEmpty()) {
        return;
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const Theme::ThemeTokens& tokens = Theme::ThemeManager::instance().tokens();
    const QRectF panel = surfaceRect();
    const QRectF content = panel.adjusted(22, 18, -22, -18);

    painter.setFont(Theme::ThemeManager::instance().font(12.0, QFont::DemiBold));
    painter.setPen(tokens.color.textSecondary);
    painter.drawText(QRectF(content.left(), content.top(), content.width(), 22),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     QStringLiteral("SWITCH WINDOWS"));

    const QRectF cardsArea(content.left(), content.top() + 32,
                           content.width(), content.height() - 58);

    const int count = m_windows.size();
    const int columns = qMin(5, count);
    const int rows = (count + columns - 1) / columns;
    const qreal gap = 10.0;
    const qreal cardWidth = (cardsArea.width() - (columns - 1) * gap) / columns;
    const qreal cardHeight = qMin<qreal>(118.0,
        (cardsArea.height() - (rows - 1) * gap) / qMax(1, rows));

    for (int index = 0; index < count; ++index) {
        const int row = index / columns;
        const int column = index % columns;
        const QRectF card(cardsArea.left() + column * (cardWidth + gap),
                          cardsArea.top() + row * (cardHeight + gap),
                          cardWidth,
                          cardHeight);
        if (card.width() < 30 || card.height() < 30) {
            continue;
        }

        const bool selected = index == m_selected;
        if (selected) {
            QColor fill = tokens.color.accent;
            fill.setAlphaF(0.24);
            painter.setPen(Qt::NoPen);
            painter.setBrush(fill);
            painter.drawRoundedRect(card, 14, 14);
            painter.setPen(QPen(tokens.color.accent, 1.8));
            painter.setBrush(Qt::NoBrush);
            painter.drawRoundedRect(card.adjusted(0.5, 0.5, -0.5, -0.5), 14, 14);
        } else {
            QColor fill = tokens.color.textPrimary;
            fill.setAlphaF(0.055);
            painter.setPen(Qt::NoPen);
            painter.setBrush(fill);
            painter.drawRoundedRect(card, 14, 14);
        }

        Compositor::AeroWindow* window = m_windows.at(index);
        if (window == nullptr) {
            continue;
        }

        const qreal iconSize = 42.0;
        const QRectF iconRect(card.center().x() - iconSize / 2.0,
                              card.top() + 12,
                              iconSize, iconSize);
        const QIcon icon = Theme::IconFactory::appIcon(window->applicationId(), 56);
        icon.paint(&painter, iconRect.toRect(), Qt::AlignCenter, QIcon::Normal, QIcon::Off);

        QFont titleFont = Theme::ThemeManager::instance().font(11.5, QFont::DemiBold);
        painter.setFont(titleFont);
        painter.setPen(tokens.color.textPrimary);
        const QRectF titleRect(card.left() + 10, iconRect.bottom() + 8,
                               card.width() - 20, 18);
        painter.drawText(titleRect, Qt::AlignHCenter | Qt::AlignVCenter,
                         painter.fontMetrics().elidedText(window->title(), Qt::ElideRight,
                                                          int(titleRect.width())));

        painter.setFont(Theme::ThemeManager::instance().captionFont());
        painter.setPen(tokens.color.textSecondary);
        const QString app = applicationName(window->applicationId(), window->title());
        const QRectF appRect(card.left() + 10, titleRect.bottom() + 1,
                             card.width() - 20, 16);
        painter.drawText(appRect, Qt::AlignHCenter | Qt::AlignVCenter,
                         painter.fontMetrics().elidedText(app, Qt::ElideRight,
                                                          int(appRect.width())));
    }

    painter.setFont(Theme::ThemeManager::instance().font(10.0, QFont::Normal));
    painter.setPen(tokens.color.textMuted);
    painter.drawText(QRectF(content.left(), content.bottom() - 18, content.width(), 18),
                     Qt::AlignHCenter | Qt::AlignVCenter,
                     QStringLiteral("Release Alt to switch  ·  Esc to cancel"));
}

} // namespace Aero::Shell
