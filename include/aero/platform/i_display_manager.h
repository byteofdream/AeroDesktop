#pragma once

#include "aero/platform/platform_types.h"
#include "aero/platform/platform_service.h"

namespace Aero::Platform {

class IDisplayManager : public PlatformService {
    Q_OBJECT
public:
    explicit IDisplayManager(QObject* parent = nullptr)
        : PlatformService(parent)
    {
        setCapability(Capability::DisplayManagement);
    }

    virtual QList<DisplayInfo> displays() const = 0;

    /// Brightness is expressed in percent (0..100).  -1 from brightness()
    /// means the display does not expose brightness control.
    virtual int brightness(const QString& displayId) const = 0;
    virtual bool setBrightness(const QString& displayId, int percent) = 0;
    virtual bool supportsBrightness() const = 0;

    virtual void refresh() = 0;

signals:
    void displaysChanged();
    void brightnessChanged(const QString& displayId, int percent);
};

} // namespace Aero::Platform
