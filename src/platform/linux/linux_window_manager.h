#pragma once

#include "aero/platform/i_window_manager.h"

#include <functional>

namespace Aero::Platform::Linux {

/// Best-effort host window integration through EWMH (via wmctrl).
///
/// This service manages *foreign* host windows only.  Aero's own application
/// windows are managed by AeroCompositor.  On Wayland the service reports
/// itself unavailable because there is no portable foreign-window protocol;
/// the interface is designed to be implemented by the DreamyOS compositor.
class LinuxWindowManager final : public IWindowManager {
    Q_OBJECT
public:
    explicit LinuxWindowManager(QObject* parent = nullptr);

    bool isAvailable() const override;
    QList<WindowInfo> windows() const override;
    WindowInfo activeWindow() const override;

    bool activateWindow(quintptr windowId) override;
    bool closeWindow(quintptr windowId) override;
    bool minimizeWindow(quintptr windowId) override;
    bool maximizeWindow(quintptr windowId, bool maximized) override;
    bool setWindowGeometry(quintptr windowId, const QRect& geometry) override;

    int workspaceCount() const override;
    int currentWorkspace() const override;
    bool setCurrentWorkspace(int index) override;

    void refresh() override;

private:
    void runWmctrl(const QStringList& arguments, std::function<void(const QByteArray&)> callback);
    void applyWindowList(const QByteArray& output);
    void applyWorkspaceList(const QByteArray& output);
    void finishStep();
    QString windowIdString(quintptr id) const;

    QString m_executable;
    QList<WindowInfo> m_windows;
    quintptr m_activeWindow = 0;
    int m_workspaceCount = 1;
    int m_currentWorkspace = 0;
    bool m_refreshing = false;
    int m_pendingSteps = 0;
};

} // namespace Aero::Platform::Linux
