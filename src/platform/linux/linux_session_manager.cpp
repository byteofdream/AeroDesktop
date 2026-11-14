#include "linux_session_manager.h"

#include "aero/core/version.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QHostInfo>
#include <QSysInfo>
#include <QThread>
#include <QtGlobal>

#include <array>
#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>

namespace Aero::Platform::Linux {
namespace {

QString readFileText(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    return QString::fromUtf8(file.readAll());
}

QHash<QString, QString> parseOsRelease()
{
    QHash<QString, QString> values;
    const QStringList lines = readFileText(QStringLiteral("/etc/os-release")).split('\n', Qt::SkipEmptyParts);
    for (const QString& line : lines) {
        const int equals = line.indexOf(QLatin1Char('='));
        if (equals <= 0) {
            continue;
        }
        QString value = line.mid(equals + 1).trimmed();
        if (value.startsWith(QLatin1Char('"')) && value.endsWith(QLatin1Char('"'))) {
            value = value.mid(1, value.size() - 2);
        }
        values.insert(line.left(equals), value);
    }
    return values;
}

UserInfo userFromStruct(const passwd& entry)
{
    UserInfo info;
    info.uid = entry.pw_uid;
    info.userName = QString::fromLocal8Bit(entry.pw_name);
    info.realName = QString::fromLocal8Bit(entry.pw_gecos).section(QLatin1Char(','), 0, 0);
    info.homePath = QString::fromLocal8Bit(entry.pw_dir);
    info.shell = QString::fromLocal8Bit(entry.pw_shell);
    const QString icon = QStringLiteral("/var/lib/AccountsService/icons/%1").arg(info.userName);
    if (QFileInfo::exists(icon)) {
        info.iconPath = icon;
    }
    return info;
}

} // namespace

LinuxSessionManager::LinuxSessionManager(QObject* parent)
    : ISessionManager(parent)
{
    QDBusConnection bus = QDBusConnection::systemBus();
    m_logind = bus.isConnected()
        && bus.interface() != nullptr
        && bus.interface()->isServiceRegistered(QStringLiteral("org.freedesktop.login1"));

    m_session.sessionType = qEnvironmentVariable("XDG_SESSION_TYPE", QStringLiteral("unknown"));
    m_session.desktopName = qEnvironmentVariable("XDG_CURRENT_DESKTOP", QStringLiteral("Aero"));
    m_session.hostName = QHostInfo::localHostName();
    m_session.seat = qEnvironmentVariable("XDG_SEAT");

    struct passwd pw {};
    struct passwd* result = nullptr;
    std::array<char, 4096> buffer {};
    if (::getpwuid_r(::getuid(), &pw, buffer.data(), buffer.size(), &result) == 0 && result != nullptr) {
        m_currentUser = userFromStruct(*result);
    }
    if (m_currentUser.userName.isEmpty()) {
        m_currentUser.userName = qEnvironmentVariable("USER", QStringLiteral("user"));
        m_currentUser.uid = ::getuid();
        m_currentUser.homePath = QDir::homePath();
    }

    readUsers();
    readSystemInfo();
}

bool LinuxSessionManager::isAvailable() const
{
    return true;
}

SessionInfo LinuxSessionManager::session() const
{
    return m_session;
}

UserInfo LinuxSessionManager::currentUser() const
{
    return m_currentUser;
}

QList<UserInfo> LinuxSessionManager::users() const
{
    return m_users;
}

SystemInfo LinuxSessionManager::systemInfo() const
{
    return m_systemInfo;
}

void LinuxSessionManager::readUsers()
{
    m_users.clear();
    QFile file(QStringLiteral("/etc/passwd"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_users.append(m_currentUser);
        return;
    }

    while (!file.atEnd()) {
        const QByteArray line = file.readLine();
        const QList<QByteArray> fields = line.split(':');
        if (fields.size() < 7) {
            continue;
        }
        bool ok = false;
        const quint32 uid = fields.at(2).toUInt(&ok);
        if (!ok || uid < 1000 || uid == 65534) {
            continue;
        }
        const QString shell = QString::fromLocal8Bit(fields.at(6));
        if (shell.endsWith(QStringLiteral("nologin")) || shell.endsWith(QStringLiteral("false"))) {
            continue;
        }
        UserInfo info;
        info.uid = uid;
        info.userName = QString::fromLocal8Bit(fields.at(0));
        info.realName = QString::fromLocal8Bit(fields.at(4)).section(QLatin1Char(','), 0, 0);
        info.homePath = QString::fromLocal8Bit(fields.at(5));
        info.shell = shell;
        const QString icon = QStringLiteral("/var/lib/AccountsService/icons/%1").arg(info.userName);
        if (QFileInfo::exists(icon)) {
            info.iconPath = icon;
        }
        m_users.append(info);
    }

    if (m_users.isEmpty()) {
        m_users.append(m_currentUser);
    }
}

void LinuxSessionManager::readSystemInfo()
{
    const QHash<QString, QString> os = parseOsRelease();
    m_systemInfo.osName = os.value(QStringLiteral("NAME"), QSysInfo::prettyProductName());
    m_systemInfo.osVersion = os.value(QStringLiteral("VERSION_ID"), QSysInfo::productVersion());
    m_systemInfo.kernelVersion = QSysInfo::kernelVersion();
    m_systemInfo.architecture = QSysInfo::currentCpuArchitecture();
    m_systemInfo.hostName = QHostInfo::localHostName();
    m_systemInfo.cpuCores = QThread::idealThreadCount();
    m_systemInfo.desktopVersion = Core::version();
    m_systemInfo.platformBackend = QStringLiteral("Linux development backend");

    const QString cpuInfo = readFileText(QStringLiteral("/proc/cpuinfo"));
    for (const QString& line : cpuInfo.split('\n')) {
        if (line.startsWith(QStringLiteral("model name"))) {
            m_systemInfo.cpuModel = line.section(QLatin1Char(':'), 1).trimmed();
            break;
        }
    }
    if (m_systemInfo.cpuModel.isEmpty()) {
        m_systemInfo.cpuModel = QStringLiteral("Unknown CPU");
    }

    const QStringList memLines = readFileText(QStringLiteral("/proc/meminfo")).split('\n');
    for (const QString& line : memLines) {
        if (line.startsWith(QStringLiteral("MemTotal:"))) {
            m_systemInfo.totalMemoryBytes = line.mid(9).trimmed().split(' ').first().toULongLong() * 1024;
            break;
        }
    }

    const QString uptime = readFileText(QStringLiteral("/proc/uptime")).trimmed();
    if (!uptime.isEmpty()) {
        m_systemInfo.uptimeSeconds = static_cast<qint64>(uptime.split(' ').first().toDouble());
    }

    const QString uevent = readFileText(QStringLiteral("/sys/class/drm/card0/device/uevent"));
    for (const QString& line : uevent.split('\n')) {
        if (line.startsWith(QStringLiteral("DRIVER="))) {
            m_systemInfo.gpuModel = line.mid(7).trimmed() + QStringLiteral(" graphics");
            break;
        }
    }
    if (m_systemInfo.gpuModel.isEmpty()) {
        m_systemInfo.gpuModel = QStringLiteral("Not reported by this system");
    }
}

void LinuxSessionManager::refresh()
{
    m_session.hostName = QHostInfo::localHostName();
    readSystemInfo();
    emit sessionChanged();
}

bool LinuxSessionManager::lockSession()
{
    if (!m_logind) {
        reportError(QStringLiteral("Session locking requires systemd-logind"));
        return false;
    }

    QDBusMessage message = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.login1"),
        QStringLiteral("/org/freedesktop/login1"),
        QStringLiteral("org.freedesktop.login1.Manager"),
        QStringLiteral("LockSessions"));
    auto* watcher = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher]() {
        const QDBusPendingReply<> reply = *watcher;
        if (reply.isError()) {
            reportError(QStringLiteral("Unable to lock the session: %1").arg(reply.error().message()));
        }
        watcher->deleteLater();
    });
    return true;
}

bool LinuxSessionManager::logoutSession()
{
    // Ending the developer session from inside an embedded shell is unsafe and
    // outside the scope of the Linux development backend.  A DreamyOS session
    // backend implements this operation against its session service.
    reportError(QStringLiteral("Logging out is not supported by the Linux development backend"));
    return false;
}

} // namespace Aero::Platform::Linux
