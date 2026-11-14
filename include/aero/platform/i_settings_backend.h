#pragma once

#include "aero/platform/platform_service.h"

namespace Aero::Platform {

/// Host system settings that Aero observes or can change.  Aero's own
/// preference model lives in AeroSettings; this interface is the narrow bridge
/// to host policy (hostname, global theme hints, etc.).
class ISettingsBackend : public PlatformService {
    Q_OBJECT
public:
    explicit ISettingsBackend(QObject* parent = nullptr)
        : PlatformService(parent)
    {
        setCapability(Capability::SystemSettings);
    }

    virtual QStringList supportedKeys() const = 0;
    virtual QVariant read(const QString& key, const QVariant& fallback = {}) const = 0;
    virtual bool write(const QString& key, const QVariant& value) = 0;
    virtual bool isWritable() const = 0;

signals:
    void settingChanged(const QString& key, const QVariant& value);
};

} // namespace Aero::Platform
