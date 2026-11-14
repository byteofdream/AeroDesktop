#pragma once

#include <QLoggingCategory>
#include <QString>

Q_DECLARE_LOGGING_CATEGORY(aeroLog)

namespace Aero::Core {

/// Installs the Aero log handler.  The handler writes to stderr and, when a
/// path is supplied, to a rotating-ish plain text log.  It deliberately avoids
/// third-party dependencies and is safe to call once at process start.
class Log {
public:
    static void install(const QString& filePath = {});
    static void shutdown();

    /// File destination used by install().  Empty when logging to stderr only.
    static QString filePath();

private:
    Log() = delete;
};

} // namespace Aero::Core
