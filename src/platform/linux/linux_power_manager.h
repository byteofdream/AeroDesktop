#pragma once

#include "aero/platform/i_power_manager.h"

namespace Aero::Platform::Linux {

class LinuxPowerManager final : public IPowerManager {
    Q_OBJECT
public:
    explicit LinuxPowerManager(QObject* parent = nullptr);

    bool isAvailable() const override;
    BatteryInfo battery() const override;
    PowerCapabilities capabilities() const override;

    bool suspend() override;
    bool hibernate() override;
    bool reboot() override;
    bool powerOff() override;
    bool lock() override;

    void refresh() override;

private:
    BatteryInfo readBattery() const;
    bool callLogind(const QString& method, const QVariantList& arguments = {});
    bool detectLogind();

    BatteryInfo m_battery;
    PowerCapabilities m_capabilities;
    bool m_logind = false;
};

} // namespace Aero::Platform::Linux
