#include "linux_bluetooth_manager.h"

#include <QProcess>
#include <QStandardPaths>
#include <QTimer>

namespace Aero::Platform::Linux {

LinuxBluetoothManager::LinuxBluetoothManager(QObject* parent)
    : IBluetoothManager(parent)
{
    m_executable = QStandardPaths::findExecutable(QStringLiteral("bluetoothctl"));
    m_info.available = !m_executable.isEmpty();
}

bool LinuxBluetoothManager::isAvailable() const
{
    return !m_executable.isEmpty();
}

BluetoothInfo LinuxBluetoothManager::state() const
{
    return m_info;
}

void LinuxBluetoothManager::runBluetoothctl(
    const QStringList& arguments,
    std::function<void(const QByteArray&, const QString&)> callback)
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
                const QString error = status == QProcess::NormalExit
                    ? QString()
                    : QString::fromLocal8Bit(process->readAllStandardError()).trimmed();
                process->deleteLater();
                if (callback) {
                    callback(output, error);
                }
            });
    process->start();
}

void LinuxBluetoothManager::refresh()
{
    if (m_refreshing || m_executable.isEmpty()) {
        if (m_executable.isEmpty()) {
            emit stateChanged(m_info);
        }
        return;
    }

    m_refreshing = true;
    m_pendingSteps = 2;

    runBluetoothctl({QStringLiteral("show")}, [this](const QByteArray& output, const QString& error) {
        if (!error.isEmpty()) {
            setLastError(error);
        }
        applyShow(output);
        finishStep();
    });

    runBluetoothctl({QStringLiteral("devices"), QStringLiteral("Connected")},
                    [this](const QByteArray& output, const QString&) {
                        applyDevices(output);
                        finishStep();
                    });
}

void LinuxBluetoothManager::finishStep()
{
    if (m_pendingSteps <= 0) {
        return;
    }
    if (--m_pendingSteps > 0) {
        return;
    }
    m_refreshing = false;
    emit stateChanged(m_info);
}

void LinuxBluetoothManager::applyShow(const QByteArray& output)
{
    const QStringList lines = QString::fromLocal8Bit(output).split('\n');
    for (const QString& line : lines) {
        const QString trimmed = line.trimmed();
        if (trimmed.startsWith(QStringLiteral("Powered:"))) {
            m_info.enabled = trimmed.mid(8).trimmed().compare(QStringLiteral("yes"), Qt::CaseInsensitive) == 0;
        }
    }
}

void LinuxBluetoothManager::applyDevices(const QByteArray& output)
{
    QList<BluetoothDevice> devices;
    const QStringList lines = QString::fromLocal8Bit(output).split('\n', Qt::SkipEmptyParts);
    for (const QString& line : lines) {
        const QStringList fields = line.simplified().split(' ');
        if (fields.size() < 3 || fields.first() != QStringLiteral("Device")) {
            continue;
        }
        BluetoothDevice device;
        device.address = fields.at(1);
        device.name = fields.mid(2).join(QLatin1Char(' '));
        device.connected = true;
        device.paired = true;
        devices.append(device);
    }
    m_info.devices = devices;
}

bool LinuxBluetoothManager::setEnabled(bool enabled)
{
    if (m_executable.isEmpty()) {
        reportError(QStringLiteral("BlueZ is not installed"));
        return false;
    }

    runBluetoothctl({QStringLiteral("power"), enabled ? QStringLiteral("on") : QStringLiteral("off")},
                    [this, enabled](const QByteArray&, const QString& error) {
                        if (!error.isEmpty()) {
                            reportError(error);
                            return;
                        }
                        m_info.enabled = enabled;
                        emit stateChanged(m_info);
                        QTimer::singleShot(400, this, &LinuxBluetoothManager::refresh);
                    });
    return true;
}

bool LinuxBluetoothManager::connectDevice(const QString& address)
{
    if (m_executable.isEmpty() || address.isEmpty()) {
        return false;
    }
    runBluetoothctl({QStringLiteral("connect"), address}, [this](const QByteArray&, const QString& error) {
        if (!error.isEmpty()) {
            reportError(error);
        }
        QTimer::singleShot(600, this, &LinuxBluetoothManager::refresh);
    });
    return true;
}

bool LinuxBluetoothManager::disconnectDevice(const QString& address)
{
    if (m_executable.isEmpty() || address.isEmpty()) {
        return false;
    }
    runBluetoothctl({QStringLiteral("disconnect"), address}, [this](const QByteArray&, const QString& error) {
        if (!error.isEmpty()) {
            reportError(error);
        }
        QTimer::singleShot(400, this, &LinuxBluetoothManager::refresh);
    });
    return true;
}

} // namespace Aero::Platform::Linux
