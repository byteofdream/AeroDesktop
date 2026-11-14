#include "linux_network_manager.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkInterface>
#include <QProcess>
#include <QStandardPaths>
#include <QTimer>

#include <algorithm>
#include <functional>

namespace Aero::Platform::Linux {
namespace {

QStringList splitNmcliLine(const QString& line)
{
    QStringList fields;
    QString current;
    bool escaped = false;

    for (const QChar character : line) {
        if (escaped) {
            current.append(character);
            escaped = false;
        } else if (character == QLatin1Char('\\')) {
            escaped = true;
        } else if (character == QLatin1Char(':')) {
            fields.append(current);
            current.clear();
        } else {
            current.append(character);
        }
    }
    fields.append(current);
    return fields;
}

bool interfaceIsWireless(const QString& name)
{
    return QFileInfo::exists(QStringLiteral("/sys/class/net/%1/wireless").arg(name));
}

} // namespace

LinuxNetworkManager::LinuxNetworkManager(QObject* parent)
    : INetworkManager(parent)
{
    m_nmcliPath = QStandardPaths::findExecutable(QStringLiteral("nmcli"));
    m_info.available = !m_nmcliPath.isEmpty() || QFileInfo::exists(QStringLiteral("/sys/class/net"));
    applyFallback();
}

bool LinuxNetworkManager::isAvailable() const
{
    return m_info.available;
}

NetworkInfo LinuxNetworkManager::network() const
{
    return m_info;
}

QList<WifiNetwork> LinuxNetworkManager::wifiNetworks() const
{
    return m_networks;
}

void LinuxNetworkManager::runNmcli(const QStringList& arguments,
                                   std::function<void(const QByteArray&, const QString&)> callback)
{
    if (m_nmcliPath.isEmpty()) {
        return;
    }

    auto* process = new QProcess(this);
    process->setProgram(m_nmcliPath);
    process->setArguments(arguments);
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [process, callback](int, QProcess::ExitStatus status) {
                const QByteArray output = process->readAllStandardOutput();
                const QString error = status == QProcess::NormalExit
                    ? QString()
                    : QString::fromLocal8Bit(process->readAllStandardError()).trimmed();
                process->deleteLater();
                if (callback) {
                    callback(output, error);
                }
            });
    connect(process, &QProcess::errorOccurred, this, [process, callback](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart) {
            return;
        }
        process->deleteLater();
        if (callback) {
            callback({}, process->errorString());
        }
    });
    process->start();
}

void LinuxNetworkManager::refresh()
{
    if (m_refreshing) {
        return;
    }
    m_refreshing = true;
    m_pendingSteps = 0;

    if (m_nmcliPath.isEmpty()) {
        applyFallback();
        m_refreshing = false;
        emit networkChanged(m_info);
        emit wifiNetworksChanged(m_networks);
        return;
    }

    m_pendingSteps = 3;

    runNmcli({QStringLiteral("-t"), QStringLiteral("-f"), QStringLiteral("WIFI"), QStringLiteral("radio")},
             [this](const QByteArray& output, const QString&) {
                 m_wifiEnabled = QString::fromLocal8Bit(output).trimmed().compare(QStringLiteral("enabled"),
                                                                                  Qt::CaseInsensitive) == 0;
                 finishStep();
             });

    runNmcli({QStringLiteral("-t"), QStringLiteral("-f"),
              QStringLiteral("DEVICE,TYPE,STATE,CONNECTION"),
              QStringLiteral("device"), QStringLiteral("status")},
             [this](const QByteArray& output, const QString&) {
                 applyDeviceList(output);
                 finishStep();
             });

    runNmcli({QStringLiteral("-t"), QStringLiteral("-f"),
              QStringLiteral("ACTIVE,SSID,SIGNAL,SECURITY"),
              QStringLiteral("device"), QStringLiteral("wifi"), QStringLiteral("list")},
             [this](const QByteArray& output, const QString&) {
                 applyWifiList(output);
                 finishStep();
             });
}

void LinuxNetworkManager::finishStep()
{
    if (m_pendingSteps <= 0) {
        return;
    }
    if (--m_pendingSteps > 0) {
        return;
    }

    m_refreshing = false;
    m_info.wifiEnabled = m_wifiEnabled;
    m_info.ipAddress = interfaceAddress(m_info.interfaceName);
    emit networkChanged(m_info);
    emit wifiNetworksChanged(m_networks);
}

void LinuxNetworkManager::applyDeviceList(const QByteArray& output)
{
    m_info.connected = false;
    m_info.wifi = false;
    m_activeInterface.clear();
    m_activeConnection.clear();

    const QStringList lines = QString::fromLocal8Bit(output).split('\n', Qt::SkipEmptyParts);
    for (const QString& line : lines) {
        const QStringList fields = splitNmcliLine(line);
        if (fields.size() < 3) {
            continue;
        }
        const QString device = fields.at(0);
        const QString type = fields.at(1);
        const QString state = fields.at(2);
        const QString connection = fields.size() > 3 ? fields.at(3) : QString();

        if (state != QStringLiteral("connected")) {
            continue;
        }

        const bool wifi = type == QStringLiteral("wifi");
        if (!m_info.connected || wifi) {
            m_info.connected = true;
            m_info.wifi = wifi;
            m_info.interfaceName = device;
            m_activeInterface = device;
            m_activeConnection = connection;
            if (!wifi) {
                m_info.ssid.clear();
                m_info.signalStrength = 0;
            }
        }
        if (wifi) {
            break;
        }
    }

    m_info.available = true;
    m_info.wifiEnabled = m_wifiEnabled;
}

void LinuxNetworkManager::applyWifiList(const QByteArray& output)
{
    m_networks.clear();
    const QStringList lines = QString::fromLocal8Bit(output).split('\n', Qt::SkipEmptyParts);

    for (const QString& line : lines) {
        const QStringList fields = splitNmcliLine(line);
        if (fields.size() < 3) {
            continue;
        }

        WifiNetwork network;
        const bool active = fields.at(0) == QStringLiteral("yes");
        network.ssid = fields.at(1);
        network.strength = fields.at(2).toInt();
        network.secured = fields.size() > 3 && !fields.at(3).isEmpty() && fields.at(3) != QStringLiteral("--");
        network.connected = active || (!m_activeConnection.isEmpty() && network.ssid == m_activeConnection);

        if (network.ssid.isEmpty()) {
            continue;
        }

        bool duplicate = false;
        for (const WifiNetwork& existing : std::as_const(m_networks)) {
            if (existing.ssid == network.ssid) {
                duplicate = true;
                break;
            }
        }
        if (duplicate) {
            continue;
        }

        if (network.connected && m_info.wifi) {
            m_info.ssid = network.ssid;
            m_info.signalStrength = network.strength;
        }
        m_networks.append(network);
    }

    std::sort(m_networks.begin(), m_networks.end(), [](const WifiNetwork& a, const WifiNetwork& b) {
        if (a.connected != b.connected) {
            return a.connected;
        }
        return a.strength > b.strength;
    });
}

void LinuxNetworkManager::applyFallback()
{
    m_info.connected = false;
    m_info.wifi = false;
    m_info.ssid.clear();
    m_info.interfaceName.clear();
    m_info.ipAddress.clear();
    m_info.signalStrength = 0;

    const QStringList interfaces = QDir(QStringLiteral("/sys/class/net")).entryList(
        QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString& name : interfaces) {
        if (name == QStringLiteral("lo")) {
            continue;
        }

        QFile stateFile(QStringLiteral("/sys/class/net/%1/operstate").arg(name));
        if (!stateFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            continue;
        }
        const QString state = QString::fromLocal8Bit(stateFile.readLine()).trimmed();
        if (state != QStringLiteral("up") && state != QStringLiteral("unknown")) {
            continue;
        }

        const bool wifi = interfaceIsWireless(name);
        m_info.connected = true;
        m_info.wifi = wifi;
        m_info.available = true;
        m_info.interfaceName = name;
        m_info.ipAddress = interfaceAddress(name);
        m_info.ssid = wifi ? QStringLiteral("Wi-Fi") : QString();
        if (wifi) {
            break;
        }
    }
}

QString LinuxNetworkManager::interfaceAddress(const QString& interfaceName) const
{
    if (interfaceName.isEmpty()) {
        return {};
    }
    const QNetworkInterface interface = QNetworkInterface::interfaceFromName(interfaceName);
    for (const QNetworkAddressEntry& entry : interface.addressEntries()) {
        if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol) {
            return entry.ip().toString();
        }
    }
    return {};
}

bool LinuxNetworkManager::setWifiEnabled(bool enabled)
{
    if (m_nmcliPath.isEmpty()) {
        reportError(QStringLiteral("NetworkManager is not installed"));
        return false;
    }

    runNmcli({QStringLiteral("radio"), QStringLiteral("wifi"),
              enabled ? QStringLiteral("on") : QStringLiteral("off")},
             [this](const QByteArray&, const QString& error) {
                 if (!error.isEmpty()) {
                     reportError(error);
                 }
                 QTimer::singleShot(400, this, &LinuxNetworkManager::refresh);
             });
    return true;
}

bool LinuxNetworkManager::connectToWifi(const QString& ssid, const QString& password)
{
    if (m_nmcliPath.isEmpty()) {
        reportError(QStringLiteral("NetworkManager is not installed"));
        return false;
    }
    if (ssid.isEmpty()) {
        reportError(QStringLiteral("A network name is required"));
        return false;
    }

    QStringList arguments = {
        QStringLiteral("--wait"), QStringLiteral("30"),
        QStringLiteral("device"), QStringLiteral("wifi"), QStringLiteral("connect"), ssid,
    };
    if (!password.isEmpty()) {
        arguments << QStringLiteral("password") << password;
    }

    runNmcli(arguments, [this](const QByteArray&, const QString& error) {
        if (!error.isEmpty()) {
            reportError(error);
        }
        QTimer::singleShot(600, this, &LinuxNetworkManager::refresh);
    });
    return true;
}

bool LinuxNetworkManager::disconnectWifi()
{
    if (m_nmcliPath.isEmpty() || m_activeInterface.isEmpty()) {
        return false;
    }
    runNmcli({QStringLiteral("device"), QStringLiteral("disconnect"), m_activeInterface},
             [this](const QByteArray&, const QString& error) {
                 if (!error.isEmpty()) {
                     reportError(error);
                 }
                 QTimer::singleShot(300, this, &LinuxNetworkManager::refresh);
             });
    return true;
}

void LinuxNetworkManager::scanWifi()
{
    if (m_nmcliPath.isEmpty()) {
        return;
    }
    runNmcli({QStringLiteral("device"), QStringLiteral("wifi"), QStringLiteral("rescan")},
             [this](const QByteArray&, const QString&) {
                 QTimer::singleShot(800, this, &LinuxNetworkManager::refresh);
             });
}

} // namespace Aero::Platform::Linux
