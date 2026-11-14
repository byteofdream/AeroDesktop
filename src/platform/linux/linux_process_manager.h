#pragma once

#include "aero/platform/i_process_manager.h"

#include <QFutureWatcher>
#include <QHash>

namespace Aero::Platform::Linux {

class LinuxProcessManager final : public IProcessManager {
    Q_OBJECT
public:
    explicit LinuxProcessManager(QObject* parent = nullptr);

    bool isAvailable() const override;

    QList<ProcessInfo> processes() const override;
    MemoryInfo memory() const override;
    double cpuUsagePercent() const override;
    qint64 uptimeSeconds() const override;

    bool killProcess(int pid, bool force) override;
    void refresh() override;

private:
    struct Snapshot {
        QList<ProcessInfo> processes;
        MemoryInfo memory;
        double cpuPercent = 0.0;
        qint64 uptimeSeconds = 0;
        qint64 timestampMs = 0;
        qint64 cpuTotalTicks = 0;
        qint64 cpuIdleTicks = 0;
        qint64 bootTimeSeconds = 0;
        QHash<int, qint64> processTicks;
    };

    static Snapshot collect(Snapshot previous);

    QFutureWatcher<Snapshot>* m_watcher = nullptr;
    QList<ProcessInfo> m_processes;
    MemoryInfo m_memory;
    double m_cpuPercent = 0.0;
    qint64 m_uptimeSeconds = 0;
    QHash<int, qint64> m_previousTicks;
    qint64 m_previousCpuTotal = 0;
    qint64 m_previousCpuIdle = 0;
    qint64 m_previousTimestampMs = 0;
    qint64 m_bootTimeSeconds = 0;
};

} // namespace Aero::Platform::Linux
