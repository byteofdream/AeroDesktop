#pragma once

#include "aero/platform/platform_service.h"

namespace Aero::Platform {

/// Host input configuration.  Global shortcut capture is not done here: the
/// shell owns its shortcuts while focused, and a future compositor backend can
/// implement system-wide grabs through this interface.
class IInputManager : public PlatformService {
    Q_OBJECT
public:
    explicit IInputManager(QObject* parent = nullptr)
        : PlatformService(parent)
    {
        setCapability(Capability::InputManagement);
    }

    /// Pointer speed in the range -1.0 .. 1.0.
    virtual bool setPointerSpeed(double speed) = 0;
    virtual bool setTapToClick(bool enabled) = 0;
    virtual bool setNaturalScroll(bool enabled) = 0;
    virtual bool setKeyRepeat(int delayMs, int intervalMs) = 0;

    virtual QString keyboardLayout() const = 0;
    virtual bool setKeyboardLayout(const QString& layout) = 0;

    virtual bool supportsGlobalShortcuts() const = 0;
};

} // namespace Aero::Platform
