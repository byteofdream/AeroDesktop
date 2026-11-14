#include "linux_input_manager.h"

#include <QtGlobal>

namespace Aero::Platform::Linux {

LinuxInputManager::LinuxInputManager(QObject* parent)
    : IInputManager(parent)
{
    setLastError(QStringLiteral("Host input configuration is not supported by the Linux development backend"));
}

bool LinuxInputManager::isAvailable() const
{
    return false;
}

bool LinuxInputManager::setPointerSpeed(double speed)
{
    (void)speed;
    reportError(QStringLiteral("Pointer acceleration is managed by the host desktop session"));
    return false;
}

bool LinuxInputManager::setTapToClick(bool enabled)
{
    (void)enabled;
    reportError(QStringLiteral("Touchpad configuration is managed by the host desktop session"));
    return false;
}

bool LinuxInputManager::setNaturalScroll(bool enabled)
{
    (void)enabled;
    reportError(QStringLiteral("Natural scrolling is managed by the host desktop session"));
    return false;
}

bool LinuxInputManager::setKeyRepeat(int delayMs, int intervalMs)
{
    (void)delayMs;
    (void)intervalMs;
    reportError(QStringLiteral("Keyboard repeat is managed by the host desktop session"));
    return false;
}

QString LinuxInputManager::keyboardLayout() const
{
    const QByteArray layout = qgetenv("XKB_DEFAULT_LAYOUT");
    return layout.isEmpty() ? QStringLiteral("us") : QString::fromLocal8Bit(layout);
}

bool LinuxInputManager::setKeyboardLayout(const QString& layout)
{
    (void)layout;
    reportError(QStringLiteral("Keyboard layout is managed by the host desktop session"));
    return false;
}

bool LinuxInputManager::supportsGlobalShortcuts() const
{
    return false;
}

} // namespace Aero::Platform::Linux
