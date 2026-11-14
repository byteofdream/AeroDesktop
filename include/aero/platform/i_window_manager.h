#pragma once

#include "aero/platform/platform_types.h"
#include "aero/platform/platform_service.h"

namespace Aero::Platform {

/// Controls foreign (host) windows.  Aero's own shell-managed windows are
/// handled by AeroCompositor; this interface exists for integration with an
/// existing desktop session and for a future DreamyOS compositor.
class IWindowManager : public PlatformService {
    Q_OBJECT
public:
    explicit IWindowManager(QObject* parent = nullptr)
        : PlatformService(parent)
    {
        setCapability(Capability::WindowManagement);
    }

    virtual QList<WindowInfo> windows() const = 0;
    virtual WindowInfo activeWindow() const = 0;

    virtual bool activateWindow(quintptr windowId) = 0;
    virtual bool closeWindow(quintptr windowId) = 0;
    virtual bool minimizeWindow(quintptr windowId) = 0;
    virtual bool maximizeWindow(quintptr windowId, bool maximized) = 0;
    virtual bool setWindowGeometry(quintptr windowId, const QRect& geometry) = 0;

    virtual int workspaceCount() const = 0;
    virtual int currentWorkspace() const = 0;
    virtual bool setCurrentWorkspace(int index) = 0;

    virtual void refresh() = 0;

signals:
    void windowsChanged();
    void activeWindowChanged(quintptr windowId);
    void currentWorkspaceChanged(int index);
};

} // namespace Aero::Platform
