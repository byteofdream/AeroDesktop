#include "aero/platform/platform.h"

#include "aero/core/logging.h"
#include "aero/platform/i_audio_manager.h"
#include "aero/platform/i_bluetooth_manager.h"
#include "aero/platform/i_display_manager.h"
#include "aero/platform/i_file_system.h"
#include "aero/platform/i_input_manager.h"
#include "aero/platform/i_network_manager.h"
#include "aero/platform/i_notification_service.h"
#include "aero/platform/i_power_manager.h"
#include "aero/platform/i_process_manager.h"
#include "aero/platform/i_session_manager.h"
#include "aero/platform/i_settings_backend.h"
#include "aero/platform/i_window_manager.h"
#include "backend_factory.h"

#include "linux/linux_audio_manager.h"
#include "linux/linux_bluetooth_manager.h"
#include "linux/linux_display_manager.h"
#include "linux/linux_file_system.h"
#include "linux/linux_input_manager.h"
#include "linux/linux_network_manager.h"
#include "linux/linux_notification_service.h"
#include "linux/linux_power_manager.h"
#include "linux/linux_process_manager.h"
#include "linux/linux_session_manager.h"
#include "linux/linux_settings_backend.h"
#include "linux/linux_window_manager.h"

namespace Aero::Platform {

std::unique_ptr<Platform> createLinuxPlatform(QObject* parent)
{
    auto platform = std::make_unique<Platform>(Backend::LinuxDev, parent);

    auto* fileSystem = new Linux::LinuxFileSystem;
    auto* windowManager = new Linux::LinuxWindowManager;
    auto* displayManager = new Linux::LinuxDisplayManager;
    auto* inputManager = new Linux::LinuxInputManager;
    auto* processManager = new Linux::LinuxProcessManager;
    auto* networkManager = new Linux::LinuxNetworkManager;
    auto* bluetoothManager = new Linux::LinuxBluetoothManager;
    auto* audioManager = new Linux::LinuxAudioManager;
    auto* powerManager = new Linux::LinuxPowerManager;
    auto* notificationService = new Linux::LinuxNotificationService;
    auto* sessionManager = new Linux::LinuxSessionManager;
    auto* settingsBackend = new Linux::LinuxSettingsBackend;

    platform->installServices(fileSystem,
                              windowManager,
                              displayManager,
                              inputManager,
                              processManager,
                              networkManager,
                              bluetoothManager,
                              audioManager,
                              powerManager,
                              notificationService,
                              sessionManager,
                              settingsBackend);

    networkManager->refresh();
    bluetoothManager->refresh();
    audioManager->refresh();
    displayManager->refresh();
    sessionManager->refresh();

    return platform;
}

} // namespace Aero::Platform
