#pragma once

#include "aero/platform/i_network_manager.h"

#include <QStringList>

#include <functional>

namespace Aero::Platform::Linux {

class LinuxNetworkManager final : public INetworkManager {
    Q_OBJECT
public:
    explicit LinuxNetworkManager(QObject* parent = nullptr);

    bool isAvailable() const override;
    NetworkInfo network() const override;
    QList<WifiNetwork> wifiNetworks() const override;

    bool setWifiEnabled(bool enabled) override;
    bool connectToWifi(const QString& ssid, const QString& password) override;
    bool disconnectWifi() override;
    void scanWifi() override;
    void refresh() override;

private:
    void runNmcli(const QStringList& arguments,
                  std::function<void(const QByteArray&, const QString&)> callback);
    void applyDeviceList(const QByteArray& output);
    void applyWifiList(const QByteArray& output);
    void applyFallback();
    void finishStep();
    QString interfaceAddress(const QString& interfaceName) const;

    QString m_nmcliPath;
    NetworkInfo m_info;
    QList<WifiNetwork> m_networks;
    QString m_activeInterface;
    QString m_activeConnection;
    int m_pendingSteps = 0;
    bool m_refreshing = false;
    bool m_wifiEnabled = false;
};

} // namespace Aero::Platform::Linux
