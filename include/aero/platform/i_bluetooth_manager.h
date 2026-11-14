#pragma once

#include "aero/platform/platform_types.h"
#include "aero/platform/platform_service.h"

namespace Aero::Platform {

class IBluetoothManager : public PlatformService {
    Q_OBJECT
public:
    explicit IBluetoothManager(QObject* parent = nullptr)
        : PlatformService(parent)
    {
        setCapability(Capability::BluetoothManagement);
    }

    virtual BluetoothInfo state() const = 0;
    virtual bool setEnabled(bool enabled) = 0;
    virtual bool connectDevice(const QString& address) = 0;
    virtual bool disconnectDevice(const QString& address) = 0;
    virtual void refresh() = 0;

signals:
    void stateChanged(const BluetoothInfo& info);
};

} // namespace Aero::Platform
