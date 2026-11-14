#pragma once

#include "aero/platform/platform_types.h"
#include "aero/platform/platform_service.h"

namespace Aero::Platform {

class INetworkManager : public PlatformService {
    Q_OBJECT
public:
    explicit INetworkManager(QObject* parent = nullptr)
        : PlatformService(parent)
    {
        setCapability(Capability::NetworkManagement);
    }

    virtual NetworkInfo network() const = 0;
    virtual QList<WifiNetwork> wifiNetworks() const = 0;

    virtual bool setWifiEnabled(bool enabled) = 0;
    virtual bool connectToWifi(const QString& ssid, const QString& password) = 0;
    virtual bool disconnectWifi() = 0;
    virtual void scanWifi() = 0;
    virtual void refresh() = 0;

signals:
    void networkChanged(const NetworkInfo& info);
    void wifiNetworksChanged(const QList<WifiNetwork>& networks);
};

} // namespace Aero::Platform
