#include "linux_settings_backend.h"

#include "aero/core/version.h"

#include <QFile>
#include <QGuiApplication>
#include <QHostInfo>
#include <QStyleHints>
#include <QSysInfo>

namespace Aero::Platform::Linux {

LinuxSettingsBackend::LinuxSettingsBackend(QObject* parent)
    : ISettingsBackend(parent)
{
}

bool LinuxSettingsBackend::isAvailable() const
{
    return true;
}

QStringList LinuxSettingsBackend::supportedKeys() const
{
    return {
        QStringLiteral("host.hostname"),
        QStringLiteral("host.operatingSystem"),
        QStringLiteral("host.kernel"),
        QStringLiteral("host.architecture"),
        QStringLiteral("host.sessionType"),
        QStringLiteral("host.desktop"),
        QStringLiteral("host.colorScheme"),
        QStringLiteral("host.platform"),
        QStringLiteral("aero.version"),
    };
}

QVariant LinuxSettingsBackend::read(const QString& key, const QVariant& fallback) const
{
    if (key == QStringLiteral("host.hostname")) {
        return QHostInfo::localHostName();
    }
    if (key == QStringLiteral("host.operatingSystem")) {
        return QSysInfo::prettyProductName();
    }
    if (key == QStringLiteral("host.kernel")) {
        return QSysInfo::kernelVersion();
    }
    if (key == QStringLiteral("host.architecture")) {
        return QSysInfo::currentCpuArchitecture();
    }
    if (key == QStringLiteral("host.sessionType")) {
        return qEnvironmentVariable("XDG_SESSION_TYPE", QStringLiteral("unknown"));
    }
    if (key == QStringLiteral("host.desktop")) {
        return qEnvironmentVariable("XDG_CURRENT_DESKTOP", QStringLiteral("Aero"));
    }
    if (key == QStringLiteral("host.colorScheme")) {
        const Qt::ColorScheme scheme = QGuiApplication::styleHints()->colorScheme();
        if (scheme == Qt::ColorScheme::Dark) {
            return QStringLiteral("dark");
        }
        if (scheme == Qt::ColorScheme::Light) {
            return QStringLiteral("light");
        }
        return QStringLiteral("unknown");
    }
    if (key == QStringLiteral("host.platform")) {
        return QGuiApplication::platformName();
    }
    if (key == QStringLiteral("aero.version")) {
        return Core::version();
    }
    return fallback;
}

bool LinuxSettingsBackend::write(const QString& key, const QVariant& value)
{
    (void)key;
    (void)value;
    reportError(QStringLiteral("Host settings are read-only in the Linux development backend"));
    return false;
}

bool LinuxSettingsBackend::isWritable() const
{
    return false;
}

} // namespace Aero::Platform::Linux
