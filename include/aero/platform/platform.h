#pragma once

#include "aero/platform/platform_types.h"
#include "aero/platform/platform_service.h"

#include <QObject>
#include <QStringList>

#include <memory>

namespace Aero::Platform {

class IFileSystem;
class IWindowManager;
class IDisplayManager;
class IInputManager;
class IProcessManager;
class INetworkManager;
class IBluetoothManager;
class IAudioManager;
class IPowerManager;
class INotificationService;
class ISessionManager;
class ISettingsBackend;

/// Aggregate of platform services.  The shell only knows this object, never a
/// concrete Linux or DreamyOS class.  All services are QObject children of the
/// Platform instance.
class Platform : public QObject {
    Q_OBJECT
public:
    explicit Platform(Backend backend, QObject* parent = nullptr);
    ~Platform() override;

    Backend backend() const;
    QString backendDisplayName() const;

    void installServices(IFileSystem* fileSystem,
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
                         ISettingsBackend* settingsBackend);

    IFileSystem* fileSystem() const;
    IWindowManager* windowManager() const;
    IDisplayManager* displayManager() const;
    IInputManager* inputManager() const;
    IProcessManager* processManager() const;
    INetworkManager* networkManager() const;
    IBluetoothManager* bluetoothManager() const;
    IAudioManager* audioManager() const;
    IPowerManager* powerManager() const;
    INotificationService* notificationService() const;
    ISessionManager* sessionManager() const;
    ISettingsBackend* settingsBackend() const;

    QList<PlatformService*> services() const;
    bool isAvailable(Capability capability) const;
    QStringList unavailableCapabilities() const;

signals:
    void serviceError(const QString& service, const QString& message);

private:
    void connectService(PlatformService* service);

    Backend m_backend;
    IFileSystem* m_fileSystem = nullptr;
    IWindowManager* m_windowManager = nullptr;
    IDisplayManager* m_displayManager = nullptr;
    IInputManager* m_inputManager = nullptr;
    IProcessManager* m_processManager = nullptr;
    INetworkManager* m_networkManager = nullptr;
    IBluetoothManager* m_bluetoothManager = nullptr;
    IAudioManager* m_audioManager = nullptr;
    IPowerManager* m_powerManager = nullptr;
    INotificationService* m_notificationService = nullptr;
    ISessionManager* m_sessionManager = nullptr;
    ISettingsBackend* m_settingsBackend = nullptr;
};

/// Creates the platform selected at build time.  LinuxDev is the default and
/// only fully implemented backend in this repository.  The DreamyOS backend is
/// an explicit scaffold that fails closed instead of pretending to work.
std::unique_ptr<Platform> createPlatform(QObject* parent = nullptr,
                                         Backend backend = Backend::LinuxDev);

} // namespace Aero::Platform
