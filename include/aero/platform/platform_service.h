#pragma once

#include "aero/platform/platform_types.h"

#include <QObject>
#include <QString>

namespace Aero::Platform {

/// Base class for every platform service.
///
/// A service advertises availability separately from individual operations:
/// this lets the shell gracefully hide controls on systems where, for example,
/// no battery or no BlueZ daemon is present, instead of presenting dead UI.
class PlatformService : public QObject {
    Q_OBJECT
public:
    explicit PlatformService(QObject* parent = nullptr);
    ~PlatformService() override;

    virtual bool isAvailable() const = 0;

    QString lastError() const;
    Capability capability() const;

signals:
    void errorOccurred(const QString& message);
    void availabilityChanged(bool available);

protected:
    void setLastError(const QString& message);
    void reportError(const QString& message);
    void setCapability(Capability capability);

private:
    QString m_lastError;
    Capability m_capability = Capability::Filesystem;
};

} // namespace Aero::Platform
