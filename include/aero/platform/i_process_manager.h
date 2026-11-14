#pragma once

#include "aero/platform/platform_types.h"
#include "aero/platform/platform_service.h"

namespace Aero::Platform {

/// Process inspection and control used by Aero Monitor and Aero Task Manager.
/// The implementation coalesces expensive reads and publishes snapshots, so
/// callers never block the GUI on /proc traversal.
class IProcessManager : public PlatformService {
    Q_OBJECT
public:
    explicit IProcessManager(QObject* parent = nullptr)
        : PlatformService(parent)
    {
        setCapability(Capability::ProcessManagement);
    }

    virtual QList<ProcessInfo> processes() const = 0;
    virtual MemoryInfo memory() const = 0;
    virtual double cpuUsagePercent() const = 0;
    virtual qint64 uptimeSeconds() const = 0;

    virtual bool killProcess(int pid, bool force) = 0;

    /// Starts an asynchronous refresh.  When it completes, processesUpdated()
    /// and/or memoryUpdated() is emitted.
    virtual void refresh() = 0;

signals:
    void processesUpdated();
    void memoryUpdated();
    void cpuUsageUpdated(double percent);
};

} // namespace Aero::Platform
