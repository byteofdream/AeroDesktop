#pragma once

#include <QString>

namespace Aero::Core {

/// Platform-neutral locations for Aero's mutable state.  The Linux backend and
/// future DreamyOS backend both honour these paths unless a platform settings
/// backend overrides them.
class Paths {
public:
    static QString configDirectory();
    static QString dataDirectory();
    static QString cacheDirectory();
    static QString logDirectory();
    static QString runtimeDirectory();

    static QString settingsFile();
    static QString notificationsFile();
    static QString calendarFile();
    static QString logFile();

    /// Expands a leading "~" and normalises separators.  Does not require the
    /// path to exist.
    static QString expand(const QString& path);

    /// Human readable form used by the file manager and Spotlight.
    static QString elideHome(const QString& absolutePath);

    static bool ensureDirectories(QString* error = nullptr);

private:
    Paths() = delete;
};

} // namespace Aero::Core
