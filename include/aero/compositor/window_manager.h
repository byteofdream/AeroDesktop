#pragma once

#include "aero/compositor/window.h"

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QWidget>

class QPropertyAnimation;

namespace Aero::Compositor {

class WorkspaceModel;

enum class SnapZone {
    None,
    Left,
    Right,
    Top,
    TopLeft,
    TopRight,
    BottomLeft,
    BottomRight,
};

/// Shell-level window manager.
///
/// It owns window placement, z-order, focus history, workspaces, maximize and
/// minimize state, snapping and close lifecycle.  Application windows are
/// surfaces hosted in the shell's workspace layer; foreign OS windows are
/// managed separately through AeroPlatform::IWindowManager.
class WindowManager : public QObject {
    Q_OBJECT
public:
    explicit WindowManager(QWidget* workspaceHost, QObject* parent = nullptr);
    ~WindowManager() override;

    WorkspaceModel* workspaces() const;

    void setWorkArea(const QRect& area);
    QRect workArea() const;

    int openWindow(const WindowOptions& options);
    bool closeWindow(int id);
    void closeActiveWindow();
    void closeAllForApplication(const QString& applicationId);

    AeroWindow* windowById(int id) const;
    QList<AeroWindow*> windows() const;
    QList<AeroWindow*> windowsForApplication(const QString& applicationId) const;
    QList<AeroWindow*> windowsOnWorkspace(int workspace) const;
    AeroWindow* activeWindow() const;
    int activeWindowId() const;
    int windowCount() const;

    void activateWindow(int id);
    void minimizeWindow(int id);
    void minimizeActiveWindow();
    void restoreWindow(int id);
    void toggleMaximize(int id);
    void maximizeWindow(int id);
    void restoreFromMaximize(int id);
    void snapWindow(int id, SnapZone zone);
    void moveWindowToWorkspace(int id, int workspace);

    void switchWorkspace(int workspace);
    void switchWorkspaceRelative(int delta);
    void setWorkspaceCount(int count);

    void cycleWindows(bool reverse = false);
    void showDesktop();
    bool isDesktopShown() const;

signals:
    void windowOpened(int id, const QString& applicationId);
    void windowClosed(int id, const QString& applicationId);
    void windowStateChanged(int id);
    void activeWindowChanged(int id);
    void workspaceChanged(int index);
    void windowListChanged();
    void desktopShownChanged(bool shown);

private:
    QRect effectiveWorkArea() const;
    void connectWindow(AeroWindow* window);
    void handleWorkspaceChanged(int index);
    void focusNextWindow();
    void raiseWindow(AeroWindow* window);
    void setActive(AeroWindow* window);
    void animateSurface(AeroWindow* window, const QRect& target, int duration);
    void animateOpacity(AeroWindow* window, qreal from, qreal to, int duration, bool hideAtEnd = false);
    void hideMinimized(AeroWindow* window);
    void restoreWindowToState(AeroWindow* window);
    void showWindowMenu(int id, const QPoint& globalPosition);
    void updateStacking(AeroWindow* window);
    QRect clampedNormalGeometry(AeroWindow* window) const;
    void clampWindowIntoWorkArea(AeroWindow* window);

    QPointer<QWidget> m_host;
    WorkspaceModel* m_workspaces = nullptr;
    QRect m_workArea;
    QList<AeroWindow*> m_windows;
    QList<int> m_stacking;
    QList<int> m_focusHistory;
    QHash<int, AeroWindow::State> m_stateBeforeMinimize;
    QSet<int> m_minimized;
    QSet<int> m_closing;
    QSet<int> m_desktopShownWindows;
    QHash<int, QPropertyAnimation*> m_surfaceAnimations;
    QHash<int, QPropertyAnimation*> m_opacityAnimations;
    int m_nextId = 1;
    int m_activeId = -1;
    bool m_desktopShown = false;
};

} // namespace Aero::Compositor
