#pragma once

#include "aero/platform/i_display_manager.h"

namespace Aero::Platform::Linux {

class LinuxDisplayManager final : public IDisplayManager {
    Q_OBJECT
public:
    explicit LinuxDisplayManager(QObject* parent = nullptr);

    bool isAvailable() const override;
    QList<DisplayInfo> displays() const override;
    int brightness(const QString& displayId) const override;
    bool setBrightness(const QString& displayId, int percent) override;
    bool supportsBrightness() const override;
    void refresh() override;

private:
    void locateBacklight();

    QList<DisplayInfo> m_displays;
    QString m_backlightPath;
    int m_backlightMax = 0;
    QString m_brightnessctlPath;
};

} // namespace Aero::Platform::Linux
