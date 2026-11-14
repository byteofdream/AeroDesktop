#pragma once

#include "aero/compositor/window_manager.h"
#include "aero/core/event_bus.h"
#include "aero/platform/platform.h"
#include "aero/services/calendar_store.h"
#include "aero/services/notification_bus.h"
#include "aero/settings/settings_manager.h"

namespace Aero::Applications {

/// Everything an application may legitimately use.  Applications never reach
/// for process-wide singletons: the shell injects the context when it creates
/// a view, which keeps applications testable and backend agnostic.
struct AppContext {
    Platform::Platform* platform = nullptr;
    Settings::SettingsManager* settings = nullptr;
    Services::NotificationBus* notifications = nullptr;
    Services::CalendarStore* calendar = nullptr;
    Core::EventBus* events = nullptr;
    Compositor::WindowManager* windowManager = nullptr;

    Platform::IFileSystem* fileSystem() const;
    Platform::IProcessManager* processManager() const;
    Platform::INetworkManager* networkManager() const;
    Platform::IBluetoothManager* bluetoothManager() const;
    Platform::IAudioManager* audioManager() const;
    Platform::IPowerManager* powerManager() const;
    Platform::IDisplayManager* displayManager() const;
    Platform::IInputManager* inputManager() const;
    Platform::ISessionManager* sessionManager() const;
    Platform::IWindowManager* hostWindowManager() const;

    bool hasFileSystem() const;
    bool hasProcessManager() const;
    bool hasNetworkManager() const;
    bool hasBluetoothManager() const;
    bool hasAudioManager() const;
    bool hasPowerManager() const;
    bool hasDisplayManager() const;
    bool hasSessionManager() const;

    void notify(const QString& title,
                const QString& body,
                const QString& iconName = {}) const;

    void launch(const QString& applicationId, const QString& argument = {}) const;
    void openFile(const QString& path) const;
};

} // namespace Aero::Applications
