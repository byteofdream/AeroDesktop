#include "linux_window_manager.h"

#include <QGuiApplication>
#include <QProcess>
#include <QStandardPaths>
#include <QTimer>

namespace Aero::Platform::Linux {

LinuxWindowManager::LinuxWindowManager(QObject* parent)
    : IWindowManager(parent)
{
    m_executable = QStandardPaths::findExecutable(QStringLiteral("wmctrl"));
}

bool LinuxWindowManager::isAvailable() const
{
    if (m_executable.isEmpty()) {
        return false;
    }
    return QGuiApplication::platformName().startsWith(QStringLiteral("xcb"), Qt::CaseInsensitive);
}

QList<WindowInfo> LinuxWindowManager::windows() const
{
    return m_windows;
}

WindowInfo LinuxWindowManager::activeWindow() const
{
    for (const WindowInfo& window : m_windows) {
        if (window.id == m_activeWindow) {
            return window;
        }
    }
    return {};
}

void LinuxWindowManager::runWmctrl(const QStringList& arguments,
                                   std::function<void(const QByteArray&)> callback)
{
    if (m_executable.isEmpty()) {
        return;
    }

    auto* process = new QProcess(this);
    process->setProgram(m_executable);
    process->setArguments(arguments);
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [process, callback](int, QProcess::ExitStatus status) {
                const QByteArray output = process->readAllStandardOutput();
                process->deleteLater();
                if (status == QProcess::NormalExit && callback) {
                    callback(output);
                }
            });
    process->start();
}

void LinuxWindowManager::refresh()
{
    if (!isAvailable() || m_refreshing) {
        return;
    }

    m_refreshing = true;
    m_pendingSteps = 2;

    runWmctrl({QStringLiteral("-l"), QStringLiteral("-p"), QStringLiteral("-G")},
              [this](const QByteArray& output) {
                  applyWindowList(output);
                  finishStep();
              });

    runWmctrl({QStringLiteral("-d")}, [this](const QByteArray& output) {
        applyWorkspaceList(output);
        finishStep();
    });
}

void LinuxWindowManager::finishStep()
{
    if (m_pendingSteps <= 0) {
        return;
    }
    if (--m_pendingSteps > 0) {
        return;
    }
    m_refreshing = false;
    emit windowsChanged();
    emit currentWorkspaceChanged(m_currentWorkspace);
}

void LinuxWindowManager::applyWindowList(const QByteArray& output)
{
    QList<WindowInfo> windows;
    const QStringList lines = QString::fromLocal8Bit(output).split('\n', Qt::SkipEmptyParts);

    for (const QString& line : lines) {
        const QStringList fields = line.simplified().split(QLatin1Char(' '));
        if (fields.size() < 8) {
            continue;
        }

        QString idText = fields.at(0);
        if (idText.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive)) {
            idText.remove(0, 2);
        }
        bool idOk = false;
        const qulonglong id = idText.toULongLong(&idOk, 16);
        if (!idOk) {
            continue;
        }

        WindowInfo info;
        info.id = static_cast<quintptr>(id);
        info.workspace = fields.at(1).toInt();
        info.geometry = QRect(fields.at(3).toInt(),
                              fields.at(4).toInt(),
                              fields.at(5).toInt(),
                              fields.at(6).toInt());
        info.title = fields.mid(8).join(QLatin1Char(' '));
        info.active = (info.id == m_activeWindow);
        windows.append(info);
    }

    m_windows = windows;
}

void LinuxWindowManager::applyWorkspaceList(const QByteArray& output)
{
    const QStringList lines = QString::fromLocal8Bit(output).split('\n', Qt::SkipEmptyParts);
    m_workspaceCount = qMax(1, lines.size());
    for (const QString& line : lines) {
        if (line.contains(QLatin1Char('*'))) {
            m_currentWorkspace = line.trimmed().section(QLatin1Char(' '), 0, 0).toInt();
            break;
        }
    }
}

QString LinuxWindowManager::windowIdString(quintptr id) const
{
    return QStringLiteral("0x%1").arg(static_cast<qulonglong>(id), 0, 16);
}

bool LinuxWindowManager::activateWindow(quintptr windowId)
{
    if (!isAvailable()) {
        return false;
    }
    runWmctrl({QStringLiteral("-i"), QStringLiteral("-a"), windowIdString(windowId)},
              [](const QByteArray&) {});
    m_activeWindow = windowId;
    for (WindowInfo& window : m_windows) {
        window.active = window.id == windowId;
    }
    emit activeWindowChanged(windowId);
    emit windowsChanged();
    return true;
}

bool LinuxWindowManager::closeWindow(quintptr windowId)
{
    if (!isAvailable()) {
        return false;
    }
    runWmctrl({QStringLiteral("-i"), QStringLiteral("-c"), windowIdString(windowId)},
              [this](const QByteArray&) {
                  QTimer::singleShot(100, this, &LinuxWindowManager::refresh);
              });
    return true;
}

bool LinuxWindowManager::minimizeWindow(quintptr windowId)
{
    if (!isAvailable()) {
        return false;
    }
    runWmctrl({QStringLiteral("-i"), QStringLiteral("-r"), windowIdString(windowId),
               QStringLiteral("-b"), QStringLiteral("add,hidden")},
              [](const QByteArray&) {});
    return true;
}

bool LinuxWindowManager::maximizeWindow(quintptr windowId, bool maximized)
{
    if (!isAvailable()) {
        return false;
    }
    runWmctrl({QStringLiteral("-i"), QStringLiteral("-r"), windowIdString(windowId),
               QStringLiteral("-b"),
               maximized
                   ? QStringLiteral("add,maximized_vert,maximized_horz")
                   : QStringLiteral("remove,maximized_vert,maximized_horz")},
              [](const QByteArray&) {});
    return true;
}

bool LinuxWindowManager::setWindowGeometry(quintptr windowId, const QRect& geometry)
{
    if (!isAvailable()) {
        return false;
    }
    runWmctrl({QStringLiteral("-i"), QStringLiteral("-r"), windowIdString(windowId),
               QStringLiteral("-e"),
               QStringLiteral("0,%1,%2,%3,%4")
                   .arg(geometry.x())
                   .arg(geometry.y())
                   .arg(geometry.width())
                   .arg(geometry.height())},
              [](const QByteArray&) {});
    return true;
}

int LinuxWindowManager::workspaceCount() const
{
    return m_workspaceCount;
}

int LinuxWindowManager::currentWorkspace() const
{
    return m_currentWorkspace;
}

bool LinuxWindowManager::setCurrentWorkspace(int index)
{
    if (!isAvailable() || index < 0 || index >= m_workspaceCount) {
        return false;
    }
    runWmctrl({QStringLiteral("-s"), QString::number(index)}, [](const QByteArray&) {});
    m_currentWorkspace = index;
    emit currentWorkspaceChanged(index);
    return true;
}

} // namespace Aero::Platform::Linux
