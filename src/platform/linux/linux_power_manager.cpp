#include "linux_power_manager.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDir>
#include <QFile>

namespace Aero::Platform::Linux {
namespace {

QString readLine(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    return QString::fromLocal8Bit(file.readLine()).trimmed();
}

qint64 readNumber(const QString& path)
{
    bool ok = false;
    const qint64 value = readLine(path).toLongLong(&ok);
    return ok ? value : -1;
}

} // namespace

LinuxPowerManager::LinuxPowerManager(QObject* parent)
    : IPowerManager(parent)
{
    m_logind = detectLogind();
    m_capabilities.canSuspend = m_logind;
    m_capabilities.canHibernate = m_logind;
    m_capabilities.canReboot = m_logind;
    m_capabilities.canPowerOff = m_logind;
    m_capabilities.canLock = m_logind;
    m_battery = readBattery();
}

bool LinuxPowerManager::isAvailable() const
{
    return true;
}

bool LinuxPowerManager::detectLogind()
{
    QDBusConnection bus = QDBusConnection::systemBus();
    if (!bus.isConnected()) {
        return false;
    }
    QDBusConnectionInterface* interface = bus.interface();
    return interface != nullptr && interface->isServiceRegistered(QStringLiteral("org.freedesktop.login1"));
}

BatteryInfo LinuxPowerManager::readBattery() const
{
    BatteryInfo info;
    QDir supplies(QStringLiteral("/sys/class/power_supply"));
    const QStringList entries = supplies.entryList(QDir::Dirs | QDir::NoDotAndDotDot);

    for (const QString& entry : entries) {
        const QString base = supplies.filePath(entry);
        if (readLine(base + QStringLiteral("/type")).compare(QStringLiteral("Battery"), Qt::CaseInsensitive) != 0) {
            continue;
        }

        info.present = true;
        info.percent = static_cast<int>(readNumber(base + QStringLiteral("/capacity")));
        info.status = readLine(base + QStringLiteral("/status"));
        info.charging = info.status.compare(QStringLiteral("Charging"), Qt::CaseInsensitive) == 0;
        info.fullyCharged = info.status.compare(QStringLiteral("Full"), Qt::CaseInsensitive) == 0;

        if (!info.charging && !info.fullyCharged) {
            qint64 energyNow = readNumber(base + QStringLiteral("/energy_now"));
            qint64 energyFull = readNumber(base + QStringLiteral("/energy_full"));
            qint64 powerNow = readNumber(base + QStringLiteral("/power_now"));

            if (energyNow < 0 || powerNow <= 0) {
                energyNow = readNumber(base + QStringLiteral("/charge_now"));
                powerNow = readNumber(base + QStringLiteral("/current_now"));
            }
            if (energyNow >= 0 && powerNow > 0) {
                info.secondsRemaining = static_cast<int>(energyNow / powerNow * 3600.0);
            }
            (void)energyFull;
        }
        break;
    }

    return info;
}

BatteryInfo LinuxPowerManager::battery() const
{
    return m_battery;
}

PowerCapabilities LinuxPowerManager::capabilities() const
{
    return m_capabilities;
}

void LinuxPowerManager::refresh()
{
    const BatteryInfo previous = m_battery;
    m_battery = readBattery();
    if (previous.present != m_battery.present
        || previous.percent != m_battery.percent
        || previous.charging != m_battery.charging
        || previous.secondsRemaining != m_battery.secondsRemaining
        || previous.status != m_battery.status) {
        emit batteryChanged(m_battery);
    }
}

bool LinuxPowerManager::callLogind(const QString& method, const QVariantList& arguments)
{
    if (!m_logind) {
        reportError(QStringLiteral("logind is not available"));
        return false;
    }

    QDBusMessage message = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.login1"),
        QStringLiteral("/org/freedesktop/login1"),
        QStringLiteral("org.freedesktop.login1.Manager"),
        method);
    message.setArguments(arguments);

    auto* watcher = new QDBusPendingCallWatcher(
        QDBusConnection::systemBus().asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher, method]() {
        const QDBusPendingReply<> reply = *watcher;
        if (reply.isError()) {
            reportError(QStringLiteral("%1 failed: %2").arg(method, reply.error().message()));
        }
        watcher->deleteLater();
    });
    return true;
}

bool LinuxPowerManager::suspend()
{
    return callLogind(QStringLiteral("Suspend"), {true});
}

bool LinuxPowerManager::hibernate()
{
    return callLogind(QStringLiteral("Hibernate"), {true});
}

bool LinuxPowerManager::reboot()
{
    return callLogind(QStringLiteral("Reboot"), {true});
}

bool LinuxPowerManager::powerOff()
{
    return callLogind(QStringLiteral("PowerOff"), {true});
}

bool LinuxPowerManager::lock()
{
    return callLogind(QStringLiteral("LockSessions"));
}

} // namespace Aero::Platform::Linux
