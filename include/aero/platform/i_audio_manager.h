#pragma once

#include "aero/platform/platform_types.h"
#include "aero/platform/platform_service.h"

namespace Aero::Platform {

class IAudioManager : public PlatformService {
    Q_OBJECT
public:
    explicit IAudioManager(QObject* parent = nullptr)
        : PlatformService(parent)
    {
        setCapability(Capability::AudioManagement);
    }

    virtual AudioInfo state() const = 0;
    virtual QList<AudioDeviceInfo> devices() const = 0;

    virtual bool setVolume(int percent) = 0;
    virtual bool setMuted(bool muted) = 0;
    virtual bool setDefaultDevice(const QString& id, bool input) = 0;
    virtual void refresh() = 0;

signals:
    void stateChanged(const AudioInfo& info);
    void devicesChanged();
};

} // namespace Aero::Platform
