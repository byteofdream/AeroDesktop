#include "aero/core/paths.h"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

namespace Aero::Core {
namespace {
constexpr const char* kAppDirectory = "aero-desktop";

QString baseDirectory(QStandardPaths::StandardLocation location)
{
    QString base = QStandardPaths::writableLocation(location);
    if (base.isEmpty()) {
        base = QDir::homePath() + QStringLiteral("/.aero");
    }
    return QDir(base).filePath(QString::fromLatin1(kAppDirectory));
}
} // namespace

QString Paths::configDirectory()
{
    return baseDirectory(QStandardPaths::GenericConfigLocation);
}

QString Paths::dataDirectory()
{
    return baseDirectory(QStandardPaths::GenericDataLocation);
}

QString Paths::cacheDirectory()
{
    return baseDirectory(QStandardPaths::GenericCacheLocation);
}

QString Paths::logDirectory()
{
    return QDir(cacheDirectory()).filePath(QStringLiteral("logs"));
}

QString Paths::runtimeDirectory()
{
    QString runtime = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (runtime.isEmpty()) {
        runtime = QDir::tempPath();
    }
    return QDir(runtime).filePath(QString::fromLatin1(kAppDirectory));
}

QString Paths::settingsFile()
{
    return QDir(configDirectory()).filePath(QStringLiteral("settings.json"));
}

QString Paths::notificationsFile()
{
    return QDir(dataDirectory()).filePath(QStringLiteral("notifications.json"));
}

QString Paths::calendarFile()
{
    return QDir(dataDirectory()).filePath(QStringLiteral("calendar.json"));
}

QString Paths::logFile()
{
    return QDir(logDirectory()).filePath(QStringLiteral("aero-desktop.log"));
}

QString Paths::expand(const QString& path)
{
    if (path.isEmpty()) {
        return {};
    }

    QString expanded = path;
    if (expanded == QStringLiteral("~")) {
        expanded = QDir::homePath();
    } else if (expanded.startsWith(QStringLiteral("~/"))) {
        expanded.remove(0, 1);
        expanded.prepend(QDir::homePath());
    }

    return QDir::cleanPath(expanded);
}

QString Paths::elideHome(const QString& absolutePath)
{
    const QString home = QDir::homePath();
    if (absolutePath == home) {
        return QStringLiteral("~");
    }
    if (absolutePath.startsWith(home + QLatin1Char('/'))) {
        return QStringLiteral("~") + absolutePath.mid(home.size());
    }
    return absolutePath;
}

bool Paths::ensureDirectories(QString* error)
{
    const QStringList directories = {
        configDirectory(),
        dataDirectory(),
        cacheDirectory(),
        logDirectory(),
        runtimeDirectory(),
    };

    for (const QString& directory : directories) {
        if (!QDir().mkpath(directory)) {
            if (error != nullptr) {
                *error = QStringLiteral("Unable to create directory %1").arg(directory);
            }
            return false;
        }
    }
    return true;
}

} // namespace Aero::Core
