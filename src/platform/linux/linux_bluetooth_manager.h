#pragma once

#include "aero/platform/i_bluetooth_manager.h"

#include <functional>

namespace Aero::Platform::Linux {

class LinuxBluetoothManager final : public IBluetoothManager {
    Q_OBJECT
public:
    explicit LinuxBluetoothManager(QObject* parent = nullptr);

    bool isAvailable() const override;
    BluetoothInfo state() const override;

    bool setEnabled(bool enabled) override;
    bool connectDevice(const QString& address) override;
    bool disconnectDevice(const QString& address) override;
    void refresh() override;

private:
    void runBluetoothctl(const QStringList& arguments,
                         std::function<void(const QByteArray&, const QString&)> callback);
    void applyShow(const QByteArray& output);
    void applyDevices(const QByteArray& output);
    void finishStep();

    QString m_executable;
    BluetoothInfo m_info;
    int m_pendingSteps = 0;
    bool m_refreshing = false;
};

} // namespace Aero::Platform::Linux
