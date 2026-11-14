#pragma once

namespace Aero::Settings::Keys {

// Appearance
inline constexpr auto AppearanceMode = "appearance.mode";
inline constexpr auto AccentColor = "appearance.accent";
inline constexpr auto FontScale = "appearance.fontScale";
inline constexpr auto ReducedMotion = "appearance.reducedMotion";
inline constexpr auto Transparency = "appearance.transparency";
inline constexpr auto Blur = "appearance.blur";
inline constexpr auto HighContrast = "appearance.highContrast";

// Wallpaper / desktop
inline constexpr auto WallpaperId = "wallpaper.id";
inline constexpr auto WallpaperPath = "wallpaper.path";
inline constexpr auto WallpaperFit = "wallpaper.fit";
inline constexpr auto DesktopShowIcons = "desktop.showIcons";
inline constexpr auto DesktopDoubleClick = "desktop.doubleClickToOpen";

// Dock
inline constexpr auto DockPosition = "dock.position";
inline constexpr auto DockIconSize = "dock.iconSize";
inline constexpr auto DockMagnification = "dock.magnification";
inline constexpr auto DockAutoHide = "dock.autoHide";
inline constexpr auto DockPinnedApps = "dock.pinnedApps";
inline constexpr auto DockShowRunning = "dock.showRunning";

// Displays
inline constexpr auto DisplayBrightness = "display.brightness";
inline constexpr auto DisplayNightShift = "display.nightShift";

// Shortcuts
inline constexpr auto ShortcutsPrefix = "shortcuts.";

// Mouse
inline constexpr auto MouseSpeed = "mouse.speed";
inline constexpr auto MouseNaturalScroll = "mouse.naturalScroll";
inline constexpr auto MouseTapToClick = "mouse.tapToClick";

// Sound
inline constexpr auto SoundVolume = "sound.volume";
inline constexpr auto SoundMuted = "sound.muted";
inline constexpr auto SoundOutputDevice = "sound.outputDevice";

// Network / Bluetooth
inline constexpr auto NetworkWifiEnabled = "network.wifiEnabled";
inline constexpr auto BluetoothEnabled = "bluetooth.enabled";

// Power
inline constexpr auto PowerSleepMinutes = "power.sleepMinutes";
inline constexpr auto PowerLowBatteryWarning = "power.lowBatteryWarning";

// Notifications
inline constexpr auto NotificationsDnd = "notifications.doNotDisturb";
inline constexpr auto NotificationsPreview = "notifications.showPreview";
inline constexpr auto NotificationsHistoryLimit = "notifications.historyLimit";

// Accessibility
inline constexpr auto AccessibilityCursorSize = "accessibility.cursorSize";
inline constexpr auto AccessibilityScreenReader = "accessibility.screenReader";

// Users
inline constexpr auto UserAvatar = "user.avatar";

} // namespace Aero::Settings::Keys
