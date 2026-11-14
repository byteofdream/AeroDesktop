#pragma once

#include "aero/platform/i_settings_backend.h"

namespace Aero::Platform::Linux {

/// Exposes read-only host facts to Aero Settings.  Aero's own preferences are
/// intentionally *not* stored here; they live in AeroSettings and are managed
/// by the shell.  Write access is reserved for a privileged DreamyOS service.
class LinuxSettingsBackend final : public ISettingsBackend {
    Q_OBJECT
public:
    explicit LinuxSettingsBackend(QObject* parent = nullptr);

    bool isAvailable() const override;
    QStringList supportedKeys() const override;
    QVariant read(const QString& key, const QVariant& fallback) const override;
    bool write(const QString& key, const QVariant& value) override;
    bool isWritable() const override;
};

} // namespace Aero::Platform::Linux
