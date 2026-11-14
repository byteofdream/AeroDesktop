#pragma once

#include "aero/platform/platform_types.h"
#include "aero/platform/platform_service.h"

namespace Aero::Platform {

class IPowerManager : public PlatformService {
    Q_OBJECT
public:
    explicit IPowerManager(QObject* parent = nullptr)
        : PlatformService(parent)
    {
        setCapability(Capability::PowerManagement);
    }

    virtual BatteryInfo battery() const = 0;
    virtual PowerCapabilities capabilities() const = 0;

    virtual bool suspend() = 0;
    virtual bool hibernate() = 0;
    virtual bool reboot() = 0;
    virtual bool powerOff() = 0;
    virtual bool lock() = 0;

    virtual void refresh() = 0;

signals:
    void batteryChanged(const BatteryInfo& info);
    void capabilitiesChanged();
};

} // namespace Aero::Platform
