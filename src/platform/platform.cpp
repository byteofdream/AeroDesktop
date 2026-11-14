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
#include "aero/platform/platform_factory.h"

namespace Aero::Platform {

QString capabilityName(Capability capability)
{
    switch (capability) {
    case Capability::WindowManagement: return QStringLiteral("Window management");
    case Capability::DisplayManagement: return QStringLiteral("Display management");
    case Capability::InputManagement: return QStringLiteral("Input management");
    case Capability::Filesystem: return QStringLiteral("Filesystem");
    case Capability::ProcessManagement: return QStringLiteral("Process management");
    case Capability::NetworkManagement: return QStringLiteral("Network management");
    case Capability::BluetoothManagement: return QStringLiteral("Bluetooth");
    case Capability::AudioManagement: return QStringLiteral("Audio");
    case Capability::PowerManagement: return QStringLiteral("Power management");
    case Capability::Notifications: return QStringLiteral("Notifications");
    case Capability::UserSessions: return QStringLiteral("User sessions");
    case Capability::SystemSettings: return QStringLiteral("System settings");
    }
    return QStringLiteral("Unknown");
}

QString backendName(Backend backend)
{
    switch (backend) {
    case Backend::LinuxDev: return QStringLiteral("Linux development backend");
    case Backend::DreamyOS: return QStringLiteral("DreamyOS backend");
    }
    return QStringLiteral("Unknown backend");
}

Platform::Platform(Backend backend, QObject* parent)
    : QObject(parent)
    , m_backend(backend)
{
}

Platform::~Platform() = default;

Backend Platform::backend() const
{
    return m_backend;
}

QString Platform::backendDisplayName() const
{
    return backendName(m_backend);
}

void Platform::connectService(PlatformService* service)
{
    if (service == nullptr) {
        return;
    }

    service->setParent(this);
    connect(service, &PlatformService::errorOccurred, this, [this, service](const QString& message) {
        qCWarning(aeroLog) << capabilityName(service->capability()) << ":" << message;
        emit serviceError(capabilityName(service->capability()), message);
    });
}

void Platform::installServices(IFileSystem* fileSystem,
                               IWindowManager* windowManager,
                               IDisplayManager* displayManager,
                               IInputManager* inputManager,
                               IProcessManager* processManager,
                               INetworkManager* networkManager,
                               IBluetoothManager* bluetoothManager,
                               IAudioManager* audioManager,
                               IPowerManager* powerManager,
                               INotificationService* notificationService,
                               ISessionManager* sessionManager,
                               ISettingsBackend* settingsBackend)
{
    m_fileSystem = fileSystem;
    m_windowManager = windowManager;
    m_displayManager = displayManager;
    m_inputManager = inputManager;
    m_processManager = processManager;
    m_networkManager = networkManager;
    m_bluetoothManager = bluetoothManager;
    m_audioManager = audioManager;
    m_powerManager = powerManager;
    m_notificationService = notificationService;
    m_sessionManager = sessionManager;
    m_settingsBackend = settingsBackend;

    for (PlatformService* service : services()) {
        connectService(service);
    }

    qCInfo(aeroLog) << "Platform installed:" << backendDisplayName();
}

IFileSystem* Platform::fileSystem() const { return m_fileSystem; }
IWindowManager* Platform::windowManager() const { return m_windowManager; }
IDisplayManager* Platform::displayManager() const { return m_displayManager; }
IInputManager* Platform::inputManager() const { return m_inputManager; }
IProcessManager* Platform::processManager() const { return m_processManager; }
INetworkManager* Platform::networkManager() const { return m_networkManager; }
IBluetoothManager* Platform::bluetoothManager() const { return m_bluetoothManager; }
IAudioManager* Platform::audioManager() const { return m_audioManager; }
IPowerManager* Platform::powerManager() const { return m_powerManager; }
INotificationService* Platform::notificationService() const { return m_notificationService; }
ISessionManager* Platform::sessionManager() const { return m_sessionManager; }
ISettingsBackend* Platform::settingsBackend() const { return m_settingsBackend; }

QList<PlatformService*> Platform::services() const
{
    return {
        m_fileSystem,
        m_windowManager,
        m_displayManager,
        m_inputManager,
        m_processManager,
        m_networkManager,
        m_bluetoothManager,
        m_audioManager,
        m_powerManager,
        m_notificationService,
        m_sessionManager,
        m_settingsBackend,
    };
}

bool Platform::isAvailable(Capability capability) const
{
    for (PlatformService* service : services()) {
        if (service != nullptr && service->capability() == capability) {
            return service->isAvailable();
        }
    }
    return false;
}

QStringList Platform::unavailableCapabilities() const
{
    QStringList unavailable;
    for (PlatformService* service : services()) {
        if (service != nullptr && !service->isAvailable()) {
            unavailable.append(capabilityName(service->capability()));
        }
    }
    return unavailable;
}

} // namespace Aero::Platform
