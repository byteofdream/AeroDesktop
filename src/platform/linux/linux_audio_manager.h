#pragma once

#include "aero/platform/i_audio_manager.h"

#include <functional>

namespace Aero::Platform::Linux {

class LinuxAudioManager final : public IAudioManager {
    Q_OBJECT
public:
    explicit LinuxAudioManager(QObject* parent = nullptr);

    bool isAvailable() const override;
    AudioInfo state() const override;
    QList<AudioDeviceInfo> devices() const override;

    bool setVolume(int percent) override;
    bool setMuted(bool muted) override;
    bool setDefaultDevice(const QString& id, bool input) override;
    void refresh() override;

private:
    enum class Backend { None, Wpctl, Pactl };

    void runCommand(const QString& program,
                    const QStringList& arguments,
                    std::function<void(const QByteArray&, const QString&)> callback);
    void refreshWpctl();
    void refreshPactl();
    void finishStep();
    void readPactlDevices();

    Backend m_backend = Backend::None;
    QString m_program;
    AudioInfo m_state;
    QList<AudioDeviceInfo> m_devices;
    int m_pendingSteps = 0;
    bool m_refreshing = false;
};

} // namespace Aero::Platform::Linux
