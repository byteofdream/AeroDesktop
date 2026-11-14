#include "aero/apps/app_registry.h"

#include "aero/platform/i_audio_manager.h"
#include "aero/platform/i_bluetooth_manager.h"
#include "aero/platform/i_display_manager.h"
#include "aero/platform/i_file_system.h"
#include "aero/platform/i_input_manager.h"
#include "aero/platform/i_network_manager.h"
#include "aero/platform/i_power_manager.h"
#include "aero/platform/i_process_manager.h"
#include "aero/platform/i_session_manager.h"
#include "aero/platform/i_window_manager.h"

namespace Aero::Applications {

Platform::IFileSystem* AppContext::fileSystem() const
{
    return platform != nullptr ? platform->fileSystem() : nullptr;
}

Platform::IProcessManager* AppContext::processManager() const
{
    return platform != nullptr ? platform->processManager() : nullptr;
}

Platform::INetworkManager* AppContext::networkManager() const
{
    return platform != nullptr ? platform->networkManager() : nullptr;
}

Platform::IBluetoothManager* AppContext::bluetoothManager() const
{
    return platform != nullptr ? platform->bluetoothManager() : nullptr;
}

Platform::IAudioManager* AppContext::audioManager() const
{
    return platform != nullptr ? platform->audioManager() : nullptr;
}

Platform::IPowerManager* AppContext::powerManager() const
{
    return platform != nullptr ? platform->powerManager() : nullptr;
}

Platform::IDisplayManager* AppContext::displayManager() const
{
    return platform != nullptr ? platform->displayManager() : nullptr;
}

Platform::IInputManager* AppContext::inputManager() const
{
    return platform != nullptr ? platform->inputManager() : nullptr;
}

Platform::ISessionManager* AppContext::sessionManager() const
{
    return platform != nullptr ? platform->sessionManager() : nullptr;
}

Platform::IWindowManager* AppContext::hostWindowManager() const
{
    return platform != nullptr ? platform->windowManager() : nullptr;
}

bool AppContext::hasFileSystem() const
{
    Platform::IFileSystem* service = fileSystem();
    return service != nullptr && service->isAvailable();
}

bool AppContext::hasProcessManager() const
{
    Platform::IProcessManager* service = processManager();
    return service != nullptr && service->isAvailable();
}

bool AppContext::hasNetworkManager() const
{
    Platform::INetworkManager* service = networkManager();
    return service != nullptr && service->isAvailable();
}

bool AppContext::hasBluetoothManager() const
{
    Platform::IBluetoothManager* service = bluetoothManager();
    return service != nullptr && service->isAvailable();
}

bool AppContext::hasAudioManager() const
{
    Platform::IAudioManager* service = audioManager();
    return service != nullptr && service->isAvailable();
}

bool AppContext::hasPowerManager() const
{
    Platform::IPowerManager* service = powerManager();
    return service != nullptr && service->isAvailable();
}

bool AppContext::hasDisplayManager() const
{
    Platform::IDisplayManager* service = displayManager();
    return service != nullptr && service->isAvailable();
}

bool AppContext::hasSessionManager() const
{
    Platform::ISessionManager* service = sessionManager();
    return service != nullptr && service->isAvailable();
}

void AppContext::notify(const QString& title, const QString& body, const QString& iconName) const
{
    if (notifications == nullptr) {
        return;
    }
    notifications->post(QStringLiteral("aero.desktop"),
                        QStringLiteral("Aero Desktop"),
                        title,
                        body,
                        iconName);
}

void AppContext::launch(const QString& applicationId, const QString& argument) const
{
    if (events == nullptr) {
        return;
    }
    QVariantMap payload;
    payload.insert(QStringLiteral("appId"), applicationId);
    payload.insert(QStringLiteral("argument"), argument);
    events->publish(Core::EventBus::LaunchApplication, payload);
}

void AppContext::openFile(const QString& path) const
{
    if (events == nullptr) {
        return;
    }
    QVariantMap payload;
    payload.insert(QStringLiteral("path"), path);
    events->publish(Core::EventBus::OpenFile, payload);
}

AppView::AppView(const AppContext& context, QWidget* parent)
    : QWidget(parent)
    , m_context(context)
{
    setAttribute(Qt::WA_TranslucentBackground, false);
    setAutoFillBackground(false);
}

AppView::~AppView() = default;

const AppContext& AppView::context() const
{
    return m_context;
}

QString AppView::windowTitle() const
{
    return m_title;
}

QString AppView::subtitle() const
{
    return m_subtitle;
}

bool AppView::canClose()
{
    return true;
}

void AppView::activate()
{
}

void AppView::setLaunchArgument(const QString& argument)
{
    m_launchArgument = argument;
}

QString AppView::launchArgument() const
{
    return m_launchArgument;
}

void AppView::setWindowTitleText(const QString& title)
{
    if (m_title == title) {
        return;
    }
    m_title = title;
    emit titleChanged(m_title);
}

void AppView::setSubtitleText(const QString& subtitle)
{
    if (m_subtitle == subtitle) {
        return;
    }
    m_subtitle = subtitle;
    emit subtitleChanged(m_subtitle);
}

AppRegistry& AppRegistry::instance()
{
    static AppRegistry registry;
    return registry;
}

bool AppRegistry::registerApp(const AppDefinition& definition)
{
    if (definition.id.isEmpty() || !definition.factory || contains(definition.id)) {
        return false;
    }
    m_apps.append(definition);
    return true;
}

bool AppRegistry::contains(const QString& appId) const
{
    for (const AppDefinition& app : m_apps) {
        if (app.id == appId) {
            return true;
        }
    }
    return false;
}

const AppDefinition* AppRegistry::find(const QString& appId) const
{
    for (const AppDefinition& app : m_apps) {
        if (app.id == appId) {
            return &app;
        }
    }
    return nullptr;
}

QList<AppDefinition> AppRegistry::apps() const
{
    return m_apps;
}

QStringList AppRegistry::ids() const
{
    QStringList result;
    result.reserve(m_apps.size());
    for (const AppDefinition& app : m_apps) {
        result.append(app.id);
    }
    return result;
}

AppView* AppRegistry::createView(const QString& appId, const AppContext& context, QWidget* parent) const
{
    const AppDefinition* definition = find(appId);
    if (definition == nullptr || !definition->factory) {
        return nullptr;
    }
    return definition->factory(context, parent);
}

} // namespace Aero::Applications
