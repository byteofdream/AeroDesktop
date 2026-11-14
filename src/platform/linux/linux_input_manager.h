#pragma once

#include "aero/platform/i_input_manager.h"

namespace Aero::Platform::Linux {

/// Linux input configuration is owned by the host session (X11/Wayland or
/// udev/libinput).  The development backend deliberately reports the service
/// as unavailable rather than pretending to reconfigure the host.  Aero still
/// persists its own pointer and keyboard preferences; a native compositor
/// backend applies them through this interface.
class LinuxInputManager final : public IInputManager {
    Q_OBJECT
public:
    explicit LinuxInputManager(QObject* parent = nullptr);

    bool isAvailable() const override;
    bool setPointerSpeed(double speed) override;
    bool setTapToClick(bool enabled) override;
    bool setNaturalScroll(bool enabled) override;
    bool setKeyRepeat(int delayMs, int intervalMs) override;
    QString keyboardLayout() const override;
    bool setKeyboardLayout(const QString& layout) override;
    bool supportsGlobalShortcuts() const override;
};

} // namespace Aero::Platform::Linux
