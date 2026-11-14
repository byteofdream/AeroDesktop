#include "aero/compositor/window_manager.h"

#include "aero/compositor/workspace_model.h"
#include "aero/core/utils.h"
#include "aero/theme/icon_factory.h"
#include "aero/theme/theme.h"

#include <QAction>
#include <QGraphicsOpacityEffect>
#include <QMenu>
#include <QPropertyAnimation>
#include <QTimer>

#include <algorithm>

namespace Aero::Compositor {
namespace {
constexpr int kWorkspaceDefaultCount = 4;
constexpr int kOpenAnimationMs = 170;
constexpr int kCloseAnimationMs = 120;
constexpr int kRestoreAnimationMs = 150;
} // namespace

WindowManager::WindowManager(QWidget* workspaceHost, QObject* parent)
    : QObject(parent)
    , m_host(workspaceHost)
    , m_workspaces(new WorkspaceModel(kWorkspaceDefaultCount, this))
{
    connect(m_workspaces, &WorkspaceModel::activeIndexChanged,
            this, &WindowManager::handleWorkspaceChanged);
}

WindowManager::~WindowManager() = default;

WorkspaceModel* WindowManager::workspaces() const
{
    return m_workspaces;
}

void WindowManager::setWorkArea(const QRect& area)
{
    m_workArea = area;
    if (m_workArea.isEmpty()) {
        return;
    }

    for (AeroWindow* window : std::as_const(m_windows)) {
        if (window == nullptr) {
            continue;
        }
        if (window->state() == AeroWindow::State::Maximized) {
            window->setSurfaceGeometry(m_workArea);
        } else {
            window->setNormalSurfaceGeometry(clampedNormalGeometry(window));
            if (!m_minimized.contains(window->id())) {
                clampWindowIntoWorkArea(window);
            }
        }
    }
}

QRect WindowManager::workArea() const
{
    return effectiveWorkArea();
}

QRect WindowManager::effectiveWorkArea() const
{
    if (m_workArea.isValid() && !m_workArea.isEmpty()) {
        return m_workArea;
    }
    if (m_host != nullptr) {
        return m_host->rect().adjusted(24, 56, -24, -24);
    }
    return QRect(0, 56, 1280, 720);
}

int WindowManager::openWindow(const WindowOptions& options)
{
    if (options.content == nullptr) {
        return -1;
    }

    const int id = m_nextId++;
    WindowOptions windowOptions = options;
    if (windowOptions.subtitle.isEmpty()) {
        windowOptions.subtitle = windowOptions.title;
    }

    auto* window = new AeroWindow(id, windowOptions, m_host);
    window->setWorkspace(m_workspaces->activeIndex());

    QRect area = effectiveWorkArea();
    QSize size = options.defaultSize.isValid() ? options.defaultSize : QSize(900, 600);
    size = size.boundedTo(area.size() - QSize(48, 48));
    size = size.expandedTo(options.minimumSize);

    const int cascade = (m_windows.size() % 6) * 24;
    QPoint topLeft(area.center().x() - size.width() / 2 + cascade,
                   area.center().y() - size.height() / 2 + cascade);
    topLeft.setX(qBound(area.left(), topLeft.x(), area.right() - size.width()));
    topLeft.setY(qBound(area.top(), topLeft.y(), area.bottom() - size.height()));

    QRect surface(topLeft, size);
    window->setNormalSurfaceGeometry(surface);
    window->setState(options.startMaximized ? AeroWindow::State::Maximized : AeroWindow::State::Normal);
    window->setSurfaceGeometry(options.startMaximized ? area : surface);
    window->show();

    m_windows.append(window);
    updateStacking(window);
    connectWindow(window);

    window->setOpacity(0.001);
    animateOpacity(window, 0.001, 1.0, kOpenAnimationMs);

    setActive(window);
    clampWindowIntoWorkArea(window);

    emit windowOpened(id, window->applicationId());
    emit windowListChanged();
    return id;
}

void WindowManager::connectWindow(AeroWindow* window)
{
    connect(window, &AeroWindow::closeRequested, this,
            [this](int id) { closeWindow(id); });
    connect(window, &AeroWindow::minimizeRequested, this,
            [this](int id) { minimizeWindow(id); });
    connect(window, &AeroWindow::maximizeRequested, this,
            [this](int id, bool maximized) {
                if (maximized) {
                    maximizeWindow(id);
                } else {
                    restoreFromMaximize(id);
                }
            });
    connect(window, &AeroWindow::activated, this, [this](int id) {
        if (!m_closing.contains(id)) {
            activateWindow(id);
        }
    });
    connect(window, &AeroWindow::dragFinished, this, [this](int id) {
        AeroWindow* target = windowById(id);
        if (target == nullptr || target->state() == AeroWindow::State::Maximized) {
            return;
        }
        const QRect area = effectiveWorkArea();
        const QRect surface = target->surfaceGeometry();

        if (surface.top() <= area.top() + 6) {
            maximizeWindow(id);
            return;
        }
        if (surface.left() <= area.left() + 6) {
            snapWindow(id, SnapZone::Left);
            return;
        }
        if (surface.right() >= area.right() - 6) {
            snapWindow(id, SnapZone::Right);
            return;
        }

        clampWindowIntoWorkArea(target);
        target->setNormalSurfaceGeometry(target->surfaceGeometry());
    });
    connect(window, &AeroWindow::geometryCommitted, this,
            [this](int id, const QRect& geometry) {
                AeroWindow* target = windowById(id);
                if (target != nullptr && target->state() == AeroWindow::State::Normal) {
                    target->setNormalSurfaceGeometry(geometry);
                    emit windowStateChanged(id);
                }
            });
    connect(window, &AeroWindow::contextMenuRequested, this,
            &WindowManager::showWindowMenu);
}

bool WindowManager::closeWindow(int id)
{
    AeroWindow* window = windowById(id);
    if (window == nullptr || m_closing.contains(id)) {
        return false;
    }
    if (!window->canClose()) {
        return false;
    }

    m_closing.insert(id);
    if (m_activeId == id) {
        m_activeId = -1;
        emit activeWindowChanged(-1);
    }

    const QString applicationId = window->applicationId();
    const bool wasMinimized = m_minimized.contains(id);
    m_minimized.remove(id);
    m_stateBeforeMinimize.remove(id);

    auto finishClose = [this, id, applicationId]() {
        AeroWindow* target = windowById(id);
        if (target == nullptr) {
            return;
        }

        m_windows.removeAll(target);
        m_stacking.removeAll(id);
        m_focusHistory.removeAll(id);
        m_closing.remove(id);
        m_minimized.remove(id);
        m_stateBeforeMinimize.remove(id);
        m_desktopShownWindows.remove(id);

        if (auto* animation = m_surfaceAnimations.take(id)) {
            animation->stop();
            animation->deleteLater();
        }
        if (auto* animation = m_opacityAnimations.take(id)) {
            animation->stop();
            animation->deleteLater();
        }

        emit windowClosed(id, applicationId);
        emit windowListChanged();
        target->hide();
        target->deleteLater();
        focusNextWindow();
    };

    if (wasMinimized) {
        finishClose();
        return true;
    }

    window->setEnabled(false);
    window->setAttribute(Qt::WA_TransparentForMouseEvents, true);

    if (auto* existing = m_opacityAnimations.take(id)) {
        existing->stop();
        existing->deleteLater();
    }

    window->setOpacity(0.999);
    auto* effect = qobject_cast<QGraphicsOpacityEffect*>(window->graphicsEffect());
    if (effect == nullptr) {
        finishClose();
        return true;
    }

    auto* animation = new QPropertyAnimation(effect, "opacity", window);
    animation->setDuration(Theme::ThemeManager::instance().duration(kCloseAnimationMs));
    animation->setStartValue(effect->opacity());
    animation->setEndValue(0.0);
    animation->setEasingCurve(QEasingCurve::InCubic);
    connect(animation, &QPropertyAnimation::finished, this, [finishClose]() { finishClose(); });
    m_opacityAnimations.insert(id, animation);
    animation->start();

    return true;
}

void WindowManager::closeActiveWindow()
{
    if (m_activeId >= 0) {
        closeWindow(m_activeId);
    }
}

void WindowManager::closeAllForApplication(const QString& applicationId)
{
    const QList<AeroWindow*> targets = windowsForApplication(applicationId);
    for (AeroWindow* window : targets) {
        if (window != nullptr) {
            closeWindow(window->id());
        }
    }
}

AeroWindow* WindowManager::windowById(int id) const
{
    for (AeroWindow* window : m_windows) {
        if (window != nullptr && window->id() == id) {
            return window;
        }
    }
    return nullptr;
}

QList<AeroWindow*> WindowManager::windows() const
{
    return m_windows;
}

QList<AeroWindow*> WindowManager::windowsForApplication(const QString& applicationId) const
{
    QList<AeroWindow*> result;
    for (AeroWindow* window : m_windows) {
        if (window != nullptr && window->applicationId() == applicationId) {
            result.append(window);
        }
    }
    return result;
}

QList<AeroWindow*> WindowManager::windowsOnWorkspace(int workspace) const
{
    QList<AeroWindow*> result;
    for (AeroWindow* window : m_windows) {
        if (window != nullptr && window->workspace() == workspace) {
            result.append(window);
        }
    }
    return result;
}

AeroWindow* WindowManager::activeWindow() const
{
    return windowById(m_activeId);
}

int WindowManager::activeWindowId() const
{
    return m_activeId;
}

int WindowManager::windowCount() const
{
    return m_windows.size();
}

void WindowManager::updateStacking(AeroWindow* window)
{
    if (window == nullptr) {
        return;
    }
    m_stacking.removeAll(window->id());
    m_stacking.append(window->id());
}

void WindowManager::raiseWindow(AeroWindow* window)
{
    if (window == nullptr || !window->isVisible()) {
        return;
    }
    window->raise();
    updateStacking(window);
}

void WindowManager::setActive(AeroWindow* window)
{
    if (window == nullptr) {
        if (m_activeId != -1) {
            AeroWindow* previous = windowById(m_activeId);
            if (previous != nullptr) {
                previous->setActive(false);
            }
            m_activeId = -1;
            emit activeWindowChanged(-1);
        }
        return;
    }

    if (m_activeId == window->id()) {
        raiseWindow(window);
        return;
    }

    if (AeroWindow* previous = windowById(m_activeId)) {
        previous->setActive(false);
    }

    m_activeId = window->id();
    window->setActive(true);
    raiseWindow(window);

    m_focusHistory.removeAll(window->id());
    m_focusHistory.prepend(window->id());
    while (m_focusHistory.size() > 32) {
        m_focusHistory.removeLast();
    }

    emit activeWindowChanged(m_activeId);
}

void WindowManager::activateWindow(int id)
{
    AeroWindow* window = windowById(id);
    if (window == nullptr || m_closing.contains(id)) {
        return;
    }

    if (m_minimized.contains(id)) {
        restoreWindow(id);
        return;
    }

    if (window->workspace() != m_workspaces->activeIndex()) {
        switchWorkspace(window->workspace());
    }

    if (!window->isVisible()) {
        window->show();
    }
    setActive(window);
}

void WindowManager::animateSurface(AeroWindow* window, const QRect& target, int duration)
{
    if (window == nullptr) {
        return;
    }

    const int id = window->id();
    if (auto* previous = m_surfaceAnimations.take(id)) {
        previous->stop();
        previous->deleteLater();
    }

    const int effectiveDuration = Theme::ThemeManager::instance().duration(duration);
    if (effectiveDuration <= 0) {
        window->setSurfaceGeometry(target);
        emit windowStateChanged(id);
        return;
    }

    auto* animation = new QPropertyAnimation(window, "surfaceGeometry", window);
    animation->setDuration(effectiveDuration);
    animation->setStartValue(window->surfaceGeometry());
    animation->setEndValue(target);
    animation->setEasingCurve(QEasingCurve::OutCubic);
    connect(animation, &QPropertyAnimation::finished, this, [this, id, animation]() {
        m_surfaceAnimations.remove(id);
        emit windowStateChanged(id);
        animation->deleteLater();
    });
    m_surfaceAnimations.insert(id, animation);
    animation->start();
}

void WindowManager::animateOpacity(AeroWindow* window, qreal from, qreal to, int duration, bool hideAtEnd)
{
    if (window == nullptr) {
        return;
    }

    const int id = window->id();
    if (auto* previous = m_opacityAnimations.take(id)) {
        previous->stop();
        previous->deleteLater();
    }

    const qreal safeFrom = Core::Utils::clamp(from, 0.001, 0.999);
    window->setOpacity(safeFrom);
    auto* effect = qobject_cast<QGraphicsOpacityEffect*>(window->graphicsEffect());
    if (effect == nullptr) {
        if (to >= 0.999) {
            window->setOpacity(1.0);
        }
        if (hideAtEnd) {
            hideMinimized(window);
        }
        return;
    }

    const int effectiveDuration = Theme::ThemeManager::instance().duration(duration);
    if (effectiveDuration <= 0) {
        if (to >= 0.999) {
            window->setOpacity(1.0);
        }
        if (hideAtEnd) {
            hideMinimized(window);
        }
        return;
    }

    auto* animation = new QPropertyAnimation(effect, "opacity", window);
    animation->setDuration(effectiveDuration);
    animation->setStartValue(effect->opacity());
    animation->setEndValue(to);
    animation->setEasingCurve(to < from ? QEasingCurve::InCubic : QEasingCurve::OutCubic);
    connect(animation, &QPropertyAnimation::finished, this,
            [this, id, window, to, hideAtEnd, animation]() {
                m_opacityAnimations.remove(id);
                if (to >= 0.999) {
                    QTimer::singleShot(0, window, [window]() { window->setOpacity(1.0); });
                }
                if (hideAtEnd) {
                    hideMinimized(window);
                }
                animation->deleteLater();
            });
    m_opacityAnimations.insert(id, animation);
    animation->start();
}

void WindowManager::minimizeWindow(int id)
{
    AeroWindow* window = windowById(id);
    if (window == nullptr || m_closing.contains(id) || m_minimized.contains(id)) {
        return;
    }

    m_stateBeforeMinimize.insert(id, window->state());
    m_minimized.insert(id);

    const QRect area = effectiveWorkArea();
    const QRect target(area.center().x() - 100, area.bottom() - 90, 200, 120);

    if (window->state() == AeroWindow::State::Maximized) {
        window->setState(AeroWindow::State::Normal);
    }

    animateSurface(window, target, 190);
    animateOpacity(window, 1.0, 0.0, 190, true);

    if (m_activeId == id) {
        m_activeId = -1;
        emit activeWindowChanged(-1);
        focusNextWindow();
    }

    emit windowStateChanged(id);
    emit windowListChanged();
}

void WindowManager::minimizeActiveWindow()
{
    if (m_activeId >= 0) {
        minimizeWindow(m_activeId);
    }
}

void WindowManager::hideMinimized(AeroWindow* window)
{
    if (window == nullptr) {
        return;
    }
    const int id = window->id();
    if (!m_minimized.contains(id) || m_closing.contains(id)) {
        return;
    }

    window->hide();
    window->setState(AeroWindow::State::Minimized);
    window->setOpacity(1.0);
    emit windowStateChanged(id);
    emit windowListChanged();
}

void WindowManager::restoreWindow(int id)
{
    AeroWindow* window = windowById(id);
    if (window == nullptr || m_closing.contains(id)) {
        return;
    }

    if (!m_minimized.contains(id) && window->isVisible()) {
        activateWindow(id);
        return;
    }

    m_minimized.remove(id);
    const AeroWindow::State prior = m_stateBeforeMinimize.take(id);
    const AeroWindow::State restoreState =
        (prior == AeroWindow::State::Maximized) ? AeroWindow::State::Maximized : AeroWindow::State::Normal;

    window->setState(restoreState);
    if (window->workspace() != m_workspaces->activeIndex()) {
        switchWorkspace(window->workspace());
    }

    window->setSurfaceGeometry(restoreState == AeroWindow::State::Maximized
                                   ? effectiveWorkArea()
                                   : clampedNormalGeometry(window));
    window->show();
    window->raise();
    animateOpacity(window, 0.0, 1.0, kRestoreAnimationMs);

    setActive(window);
    emit windowStateChanged(id);
    emit windowListChanged();
}

void WindowManager::maximizeWindow(int id)
{
    AeroWindow* window = windowById(id);
    if (window == nullptr || window->state() == AeroWindow::State::Maximized || m_closing.contains(id)) {
        return;
    }

    window->setNormalSurfaceGeometry(window->surfaceGeometry());
    window->setState(AeroWindow::State::Maximized);
    animateSurface(window, effectiveWorkArea(), 210);
    emit windowStateChanged(id);
}

void WindowManager::restoreFromMaximize(int id)
{
    AeroWindow* window = windowById(id);
    if (window == nullptr || window->state() != AeroWindow::State::Maximized) {
        return;
    }

    const QRect normal = clampedNormalGeometry(window);
    window->setState(AeroWindow::State::Normal);
    animateSurface(window, normal, 210);
    emit windowStateChanged(id);
}

void WindowManager::toggleMaximize(int id)
{
    AeroWindow* window = windowById(id);
    if (window == nullptr) {
        return;
    }
    if (window->state() == AeroWindow::State::Maximized) {
        restoreFromMaximize(id);
    } else {
        maximizeWindow(id);
    }
}

void WindowManager::snapWindow(int id, SnapZone zone)
{
    AeroWindow* window = windowById(id);
    if (window == nullptr) {
        return;
    }

    if (zone == SnapZone::Top) {
        maximizeWindow(id);
        return;
    }

    const QRect area = effectiveWorkArea();
    QRect target = area;
    const int halfW = area.width() / 2;
    const int halfH = area.height() / 2;

    switch (zone) {
    case SnapZone::Left:
        target.setWidth(halfW);
        break;
    case SnapZone::Right:
        target.setLeft(area.left() + halfW);
        target.setWidth(area.width() - halfW);
        break;
    case SnapZone::TopLeft:
        target.setSize(QSize(halfW, halfH));
        break;
    case SnapZone::TopRight:
        target.setTopLeft(QPoint(area.left() + halfW, area.top()));
        target.setSize(QSize(area.width() - halfW, halfH));
        break;
    case SnapZone::BottomLeft:
        target.setTopLeft(QPoint(area.left(), area.top() + halfH));
        target.setSize(QSize(halfW, area.height() - halfH));
        break;
    case SnapZone::BottomRight:
        target.setTopLeft(QPoint(area.left() + halfW, area.top() + halfH));
        target.setSize(QSize(area.width() - halfW, area.height() - halfH));
        break;
    case SnapZone::None:
    case SnapZone::Top:
        return;
    }

    if (window->state() == AeroWindow::State::Maximized) {
        window->setState(AeroWindow::State::Normal);
    }
    window->setNormalSurfaceGeometry(target);
    animateSurface(window, target, 200);
    setActive(window);
    emit windowStateChanged(id);
    emit windowListChanged();
}

void WindowManager::moveWindowToWorkspace(int id, int workspace)
{
    AeroWindow* window = windowById(id);
    if (window == nullptr || !m_workspaces->isValidIndex(workspace)) {
        return;
    }

    window->setWorkspace(workspace);
    if (workspace != m_workspaces->activeIndex()) {
        window->hide();
        if (m_activeId == id) {
            m_activeId = -1;
            emit activeWindowChanged(-1);
            focusNextWindow();
        }
    }
    emit windowListChanged();
}

void WindowManager::switchWorkspace(int workspace)
{
    if (!m_workspaces->isValidIndex(workspace) || workspace == m_workspaces->activeIndex()) {
        return;
    }
    m_workspaces->setActiveIndex(workspace);
}

void WindowManager::switchWorkspaceRelative(int delta)
{
    int next = m_workspaces->activeIndex() + delta;
    const int count = m_workspaces->count();
    next = (next % count + count) % count;
    switchWorkspace(next);
}

void WindowManager::setWorkspaceCount(int count)
{
    m_workspaces->setCount(count);
    for (AeroWindow* window : std::as_const(m_windows)) {
        if (window != nullptr && window->workspace() >= m_workspaces->count()) {
            window->setWorkspace(m_workspaces->count() - 1);
            window->hide();
        }
    }
    emit windowListChanged();
}

void WindowManager::handleWorkspaceChanged(int index)
{
    for (AeroWindow* window : std::as_const(m_windows)) {
        if (window == nullptr || m_closing.contains(window->id())) {
            continue;
        }
        if (m_minimized.contains(window->id())) {
            window->hide();
            continue;
        }
        if (window->workspace() == index) {
            window->show();
        } else {
            window->hide();
        }
    }

    if (AeroWindow* current = windowById(m_activeId)) {
        if (current->workspace() == index && !m_minimized.contains(current->id())) {
            current->raise();
        } else {
            m_activeId = -1;
        }
    }

    if (m_activeId == -1) {
        for (int id : std::as_const(m_focusHistory)) {
            AeroWindow* candidate = windowById(id);
            if (candidate != nullptr
                && candidate->workspace() == index
                && !m_minimized.contains(id)
                && !m_closing.contains(id)) {
                setActive(candidate);
                break;
            }
        }
    }

    if (m_activeId == -1) {
        for (auto it = m_stacking.crbegin(); it != m_stacking.crend(); ++it) {
            AeroWindow* candidate = windowById(*it);
            if (candidate != nullptr && candidate->workspace() == index
                && !m_minimized.contains(candidate->id())) {
                setActive(candidate);
                break;
            }
        }
    }

    emit workspaceChanged(index);
    emit windowListChanged();
}

void WindowManager::focusNextWindow()
{
    const int activeWorkspace = m_workspaces->activeIndex();

    auto isCandidate = [this, activeWorkspace](int id) {
        AeroWindow* window = windowById(id);
        return window != nullptr
            && window->workspace() == activeWorkspace
            && !m_minimized.contains(id)
            && !m_closing.contains(id)
            && id != m_activeId;
    };

    for (int id : std::as_const(m_focusHistory)) {
        if (isCandidate(id)) {
            setActive(windowById(id));
            return;
        }
    }

    for (auto it = m_stacking.crbegin(); it != m_stacking.crend(); ++it) {
        if (isCandidate(*it)) {
            setActive(windowById(*it));
            return;
        }
    }

    setActive(nullptr);
}

void WindowManager::cycleWindows(bool reverse)
{
    QList<int> candidates;
    auto addCandidate = [&candidates](int id) {
        if (!candidates.contains(id)) {
            candidates.append(id);
        }
    };

    for (int id : std::as_const(m_focusHistory)) {
        addCandidate(id);
    }
    for (auto it = m_stacking.crbegin(); it != m_stacking.crend(); ++it) {
        addCandidate(*it);
    }

    candidates.removeAll(m_activeId);
    if (candidates.isEmpty()) {
        return;
    }

    const int index = reverse ? candidates.size() - 1 : 0;
    activateWindow(candidates.at(index));
}

void WindowManager::showDesktop()
{
    if (!m_desktopShown) {
        m_desktopShownWindows.clear();
        const int activeWorkspace = m_workspaces->activeIndex();
        const QList<AeroWindow*> visible = windowsOnWorkspace(activeWorkspace);
        for (AeroWindow* window : visible) {
            if (window != nullptr && !m_minimized.contains(window->id())) {
                m_desktopShownWindows.insert(window->id());
                minimizeWindow(window->id());
            }
        }
        m_desktopShown = true;
    } else {
        const QList<int> ids = m_desktopShownWindows.values();
        m_desktopShownWindows.clear();
        for (int id : ids) {
            restoreWindow(id);
        }
        m_desktopShown = false;
    }
    emit desktopShownChanged(m_desktopShown);
}

bool WindowManager::isDesktopShown() const
{
    return m_desktopShown;
}

QRect WindowManager::clampedNormalGeometry(AeroWindow* window) const
{
    if (window == nullptr) {
        return {};
    }

    const QRect area = effectiveWorkArea();
    QRect normal = window->normalSurfaceGeometry();
    if (normal.width() <= 0 || normal.height() <= 0) {
        normal = QRect(area.center() - QPoint(450, 300), QSize(900, 600));
    }

    QSize size = normal.size().boundedTo(area.size());
    size = size.expandedTo(window->minimumSizeHint());
    QPoint topLeft = normal.topLeft();
    topLeft.setX(qBound(area.left(), topLeft.x(), qMax(area.left(), area.right() - size.width())));
    topLeft.setY(qBound(area.top(), topLeft.y(), qMax(area.top(), area.bottom() - size.height())));
    return QRect(topLeft, size);
}

void WindowManager::clampWindowIntoWorkArea(AeroWindow* window)
{
    if (window == nullptr || window->state() == AeroWindow::State::Maximized
        || m_minimized.contains(window->id())) {
        return;
    }

    const QRect target = clampedNormalGeometry(window);
    window->setNormalSurfaceGeometry(target);
    window->setSurfaceGeometry(target);
}

void WindowManager::showWindowMenu(int id, const QPoint& globalPosition)
{
    AeroWindow* window = windowById(id);
    if (window == nullptr) {
        return;
    }

    QMenu menu;
    QAction* minimize = menu.addAction(Theme::IconFactory::icon(QStringLiteral("minus"), 16),
                                       QStringLiteral("Minimize"));
    QAction* maximize = menu.addAction(
        Theme::IconFactory::icon(window->state() == AeroWindow::State::Maximized
                                     ? QStringLiteral("square")
                                     : QStringLiteral("square"),
                                 16),
        window->state() == AeroWindow::State::Maximized ? QStringLiteral("Restore") : QStringLiteral("Maximize"));
    QAction* left = menu.addAction(QStringLiteral("Tile Left"));
    QAction* right = menu.addAction(QStringLiteral("Tile Right"));
    menu.addSeparator();

    QMenu* workspaces = menu.addMenu(QStringLiteral("Move to Desktop"));
    QList<QAction*> workspaceActions;
    for (int i = 0; i < m_workspaces->count(); ++i) {
        QAction* action = workspaces->addAction(m_workspaces->name(i));
        action->setCheckable(true);
        action->setChecked(i == window->workspace());
        workspaceActions.append(action);
    }

    menu.addSeparator();
    QAction* close = menu.addAction(Theme::IconFactory::icon(QStringLiteral("close"), 16),
                                    QStringLiteral("Close"));

    QAction* chosen = menu.exec(globalPosition);
    if (chosen == nullptr) {
        return;
    }
    if (chosen == minimize) {
        minimizeWindow(id);
    } else if (chosen == maximize) {
        toggleMaximize(id);
    } else if (chosen == left) {
        snapWindow(id, SnapZone::Left);
    } else if (chosen == right) {
        snapWindow(id, SnapZone::Right);
    } else if (chosen == close) {
        closeWindow(id);
    } else {
        const int workspaceIndex = workspaceActions.indexOf(chosen);
        if (workspaceIndex >= 0) {
            moveWindowToWorkspace(id, workspaceIndex);
        }
    }
}

} // namespace Aero::Compositor
