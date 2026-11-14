#include "aero/platform/platform_service.h"

namespace Aero::Platform {

PlatformService::PlatformService(QObject* parent)
    : QObject(parent)
{
}

PlatformService::~PlatformService() = default;

QString PlatformService::lastError() const
{
    return m_lastError;
}

Capability PlatformService::capability() const
{
    return m_capability;
}

void PlatformService::setLastError(const QString& message)
{
    m_lastError = message;
}

void PlatformService::reportError(const QString& message)
{
    setLastError(message);
    emit errorOccurred(message);
}

void PlatformService::setCapability(Capability capability)
{
    m_capability = capability;
}

} // namespace Aero::Platform
