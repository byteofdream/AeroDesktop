#include "linux_process_manager.h"

#include "aero/core/logging.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QThread>
#include <QtConcurrent/QtConcurrentRun>

#include <cerrno>
#include <csignal>
#include <cstring>
#include <unistd.h>

namespace Aero::Platform::Linux {
namespace {

constexpr qint64 kPageSize = 4096;

QByteArray readSmallFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.readAll();
}

QHash<quint32, QString> userNames()
{
    QHash<quint32, QString> users;
    QFile file(QStringLiteral("/etc/passwd"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return users;
    }

    while (!file.atEnd()) {
        const QByteArray line = file.readLine();
        const QList<QByteArray> fields = line.split(':');
        if (fields.size() >= 3) {
            bool ok = false;
            const quint32 uid = fields.at(2).toUInt(&ok);
            if (ok) {
                users.insert(uid, QString::fromLocal8Bit(fields.at(0)));
            }
        }
    }
    return users;
}

QString parseUserName(const QByteArray& status, const QHash<quint32, QString>& users)
{
    const QList<QByteArray> lines = status.split('\n');
    for (const QByteArray& line : lines) {
        if (line.startsWith("Uid:")) {
            const QList<QByteArray> parts = line.mid(4).simplified().split(' ');
            if (!parts.isEmpty()) {
                const quint32 uid = parts.first().toUInt();
                return users.value(uid, QString::number(uid));
            }
        }
    }
    return QStringLiteral("unknown");
}

} // namespace

LinuxProcessManager::LinuxProcessManager(QObject* parent)
    : IProcessManager(parent)
    , m_watcher(new QFutureWatcher<Snapshot>(this))
{
    connect(m_watcher, &QFutureWatcher<Snapshot>::finished, this, [this]() {
        const Snapshot snapshot = m_watcher->result();
        m_processes = snapshot.processes;
        m_memory = snapshot.memory;
        m_cpuPercent = snapshot.cpuPercent;
        m_uptimeSeconds = snapshot.uptimeSeconds;
        m_previousTicks = snapshot.processTicks;
        m_previousCpuTotal = snapshot.cpuTotalTicks;
        m_previousCpuIdle = snapshot.cpuIdleTicks;
        m_previousTimestampMs = snapshot.timestampMs;
        m_bootTimeSeconds = snapshot.bootTimeSeconds;
        emit processesUpdated();
        emit memoryUpdated();
        emit cpuUsageUpdated(m_cpuPercent);
    });
}

bool LinuxProcessManager::isAvailable() const
{
    return QFileInfo::exists(QStringLiteral("/proc/self/stat"));
}

void LinuxProcessManager::refresh()
{
    if (m_watcher->isRunning()) {
        return;
    }

    Snapshot previous;
    previous.processTicks = m_previousTicks;
    previous.cpuTotalTicks = m_previousCpuTotal;
    previous.cpuIdleTicks = m_previousCpuIdle;
    previous.timestampMs = m_previousTimestampMs;
    previous.bootTimeSeconds = m_bootTimeSeconds;

    m_watcher->setFuture(QtConcurrent::run(&LinuxProcessManager::collect, previous));
}

QList<ProcessInfo> LinuxProcessManager::processes() const
{
    return m_processes;
}

MemoryInfo LinuxProcessManager::memory() const
{
    return m_memory;
}

double LinuxProcessManager::cpuUsagePercent() const
{
    return m_cpuPercent;
}

qint64 LinuxProcessManager::uptimeSeconds() const
{
    return m_uptimeSeconds;
}

bool LinuxProcessManager::killProcess(int pid, bool force)
{
    if (pid <= 1) {
        reportError(QStringLiteral("Refusing to terminate a system process"));
        return false;
    }
    if (pid == static_cast<int>(::getpid())) {
        reportError(QStringLiteral("Refusing to terminate Aero Desktop"));
        return false;
    }
    if (::kill(static_cast<pid_t>(pid), force ? SIGKILL : SIGTERM) != 0) {
        reportError(QStringLiteral("Unable to terminate PID %1: %2")
                        .arg(pid)
                        .arg(QString::fromLocal8Bit(std::strerror(errno))));
        return false;
    }
    return true;
}

LinuxProcessManager::Snapshot LinuxProcessManager::collect(Snapshot previous)
{
    Snapshot snapshot;
    snapshot.timestampMs = QDateTime::currentMSecsSinceEpoch();
    snapshot.processTicks = previous.processTicks;
    snapshot.cpuTotalTicks = previous.cpuTotalTicks;
    snapshot.cpuIdleTicks = previous.cpuIdleTicks;
    snapshot.timestampMs = QDateTime::currentMSecsSinceEpoch();

    // ---- CPU / aggregate -------------------------------------------------
    const QByteArray stat = readSmallFile(QStringLiteral("/proc/stat"));
    for (const QByteArray& line : stat.split('\n')) {
        if (line.startsWith("cpu ")) {
            const QList<QByteArray> fields = line.mid(4).simplified().split(' ');
            qint64 total = 0;
            for (const QByteArray& field : fields) {
                total += field.toLongLong();
            }
            const qint64 idle = fields.size() > 3
                ? fields.at(3).toLongLong() + (fields.size() > 4 ? fields.at(4).toLongLong() : 0)
                : 0;
            const qint64 deltaTotal = total - previous.cpuTotalTicks;
            const qint64 deltaIdle = idle - previous.cpuIdleTicks;
            if (previous.cpuTotalTicks > 0 && deltaTotal > 0) {
                snapshot.cpuPercent = qBound(0.0,
                                             100.0 * (1.0 - static_cast<double>(deltaIdle) / deltaTotal),
                                             100.0);
            }
            snapshot.cpuTotalTicks = total;
            snapshot.cpuIdleTicks = idle;
        } else if (line.startsWith("btime ")) {
            snapshot.bootTimeSeconds = line.mid(6).trimmed().toLongLong();
        }
    }

    // ---- Memory ----------------------------------------------------------
    const QByteArray meminfo = readSmallFile(QStringLiteral("/proc/meminfo"));
    quint64 memTotal = 0;
    quint64 memAvailable = 0;
    quint64 buffers = 0;
    quint64 cached = 0;
    quint64 swapTotal = 0;
    quint64 swapFree = 0;

    for (const QByteArray& line : meminfo.split('\n')) {
        const int colon = line.indexOf(':');
        if (colon <= 0) {
            continue;
        }
        const QByteArray key = line.left(colon);
        const quint64 value = line.mid(colon + 1).trimmed().split(' ').first().toULongLong() * 1024ULL;
        if (key == "MemTotal") memTotal = value;
        else if (key == "MemAvailable") memAvailable = value;
        else if (key == "Buffers") buffers = value;
        else if (key == "Cached") cached = value;
        else if (key == "SwapTotal") swapTotal = value;
        else if (key == "SwapFree") swapFree = value;
    }

    snapshot.memory.totalBytes = memTotal;
    snapshot.memory.availableBytes = memAvailable;
    snapshot.memory.usedBytes = memTotal > memAvailable ? memTotal - memAvailable : 0;
    snapshot.memory.cachedBytes = buffers + cached;
    snapshot.memory.swapTotalBytes = swapTotal;
    snapshot.memory.swapUsedBytes = swapTotal > swapFree ? swapTotal - swapFree : 0;

    const QByteArray uptime = readSmallFile(QStringLiteral("/proc/uptime")).trimmed();
    if (!uptime.isEmpty()) {
        snapshot.uptimeSeconds = static_cast<qint64>(uptime.split(' ').first().toDouble());
    }

    // ---- Processes -------------------------------------------------------
    const QHash<quint32, QString> users = userNames();
    const int cores = qMax(1, QThread::idealThreadCount());
    const long ticksPerSecond = ::sysconf(_SC_CLK_TCK);
    const double elapsedSeconds = previous.timestampMs > 0
        ? (snapshot.timestampMs - previous.timestampMs) / 1000.0
        : 0.0;

    const QStringList entries = QDir(QStringLiteral("/proc")).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    snapshot.processes.reserve(entries.size());

    for (const QString& entry : entries) {
        bool pidOk = false;
        const int pid = entry.toInt(&pidOk);
        if (!pidOk) {
            continue;
        }

        const QByteArray pidStat = readSmallFile(QStringLiteral("/proc/%1/stat").arg(pid));
        if (pidStat.isEmpty()) {
            continue;
        }

        const int openParen = pidStat.indexOf('(');
        const int closeParen = pidStat.lastIndexOf(')');
        if (openParen < 0 || closeParen <= openParen) {
            continue;
        }

        const QString name = QString::fromLocal8Bit(pidStat.mid(openParen + 1, closeParen - openParen - 1));
        const QList<QByteArray> fields = pidStat.mid(closeParen + 2).simplified().split(' ');
        if (fields.size() < 22) {
            continue;
        }

        ProcessInfo process;
        process.pid = pid;
        process.name = name;
        process.state = QString::fromLatin1(fields.at(0));
        process.parentPid = fields.at(1).toInt();

        const qint64 utime = fields.at(11).toLongLong();
        const qint64 stime = fields.at(12).toLongLong();
        const qint64 startTicks = fields.at(19).toLongLong();
        const qint64 rssPages = fields.at(21).toLongLong();
        const qint64 ticks = utime + stime;

        process.residentBytes = rssPages * kPageSize;
        process.virtualBytes = 0;
        if (snapshot.bootTimeSeconds > 0 && ticksPerSecond > 0) {
            process.startedAtMs = (snapshot.bootTimeSeconds + startTicks / ticksPerSecond) * 1000;
        }

        if (previous.processTicks.contains(pid) && elapsedSeconds > 0.5 && ticksPerSecond > 0) {
            const qint64 deltaTicks = ticks - previous.processTicks.value(pid);
            process.cpuPercent = qBound(0.0,
                                        (static_cast<double>(deltaTicks) / ticksPerSecond / elapsedSeconds) * 100.0,
                                        static_cast<double>(cores) * 100.0);
        }
        snapshot.processTicks.insert(pid, ticks);

        const QByteArray status = readSmallFile(QStringLiteral("/proc/%1/status").arg(pid));
        for (const QByteArray& line : status.split('\n')) {
            if (line.startsWith("Uid:")) {
                const QList<QByteArray> parts = line.mid(4).simplified().split(' ');
                if (!parts.isEmpty()) {
                    process.user = users.value(parts.first().toUInt(), QString::number(parts.first().toUInt()));
                }
            } else if (line.startsWith("VmSize:")) {
                process.virtualBytes = line.mid(7).trimmed().split(' ').first().toLongLong() * 1024;
            }
        }
        if (process.user.isEmpty()) {
            process.user = parseUserName(status, users);
        }

        QByteArray cmdline = readSmallFile(QStringLiteral("/proc/%1/cmdline").arg(pid));
        cmdline.replace('\0', ' ');
        process.commandLine = QString::fromLocal8Bit(cmdline).trimmed();
        if (process.commandLine.isEmpty()) {
            process.commandLine = QStringLiteral("[%1]").arg(name);
        }

        snapshot.processes.append(process);
    }

    // Memory percentage needs the final total memory value.
    for (ProcessInfo& process : snapshot.processes) {
        process.memoryPercent = snapshot.memory.totalBytes > 0
            ? 100.0 * static_cast<double>(process.residentBytes) / snapshot.memory.totalBytes
            : 0.0;
    }

    return snapshot;
}

} // namespace Aero::Platform::Linux
