#include "linux_audio_manager.h"

#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>

#include <algorithm>
#include <functional>

namespace Aero::Platform::Linux {
namespace {
const QRegularExpression& percentExpression()
{
    static const QRegularExpression expression(QStringLiteral("(\\d+)%"));
    return expression;
}
} // namespace

LinuxAudioManager::LinuxAudioManager(QObject* parent)
    : IAudioManager(parent)
{
    const QString wpctl = QStandardPaths::findExecutable(QStringLiteral("wpctl"));
    if (!wpctl.isEmpty()) {
        m_backend = Backend::Wpctl;
        m_program = wpctl;
    } else {
        const QString pactl = QStandardPaths::findExecutable(QStringLiteral("pactl"));
        if (!pactl.isEmpty()) {
            m_backend = Backend::Pactl;
            m_program = pactl;
        }
    }
}

bool LinuxAudioManager::isAvailable() const
{
    return m_backend != Backend::None;
}

AudioInfo LinuxAudioManager::state() const
{
    return m_state;
}

QList<AudioDeviceInfo> LinuxAudioManager::devices() const
{
    return m_devices;
}

void LinuxAudioManager::runCommand(
    const QString& program,
    const QStringList& arguments,
    std::function<void(const QByteArray&, const QString&)> callback)
{
    auto* process = new QProcess(this);
    process->setProgram(program);
    process->setArguments(arguments);
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [process, callback](int, QProcess::ExitStatus status) {
                const QByteArray output = process->readAllStandardOutput();
                const QString error = status == QProcess::NormalExit
                    ? QString()
                    : QString::fromLocal8Bit(process->readAllStandardError()).trimmed();
                process->deleteLater();
                if (callback) {
                    callback(output, error);
                }
            });
    process->start();
}

void LinuxAudioManager::refresh()
{
    if (m_refreshing || m_backend == Backend::None) {
        if (m_backend == Backend::None) {
            emit stateChanged(m_state);
        }
        return;
    }

    m_refreshing = true;
    if (m_backend == Backend::Wpctl) {
        refreshWpctl();
    } else {
        refreshPactl();
    }
}

void LinuxAudioManager::refreshWpctl()
{
    m_pendingSteps = 1;
    runCommand(m_program, {QStringLiteral("get-volume"), QStringLiteral("@DEFAULT_AUDIO_SINK@")},
               [this](const QByteArray& output, const QString& error) {
                   if (!error.isEmpty()) {
                       setLastError(error);
                   } else {
                       const QString text = QString::fromLocal8Bit(output);
                       const QRegularExpressionMatch match = percentExpression().match(text);
                       static const QRegularExpression volumeExpression(
                           QStringLiteral("Volume:\\s*([0-9.]+)"));
                       const QRegularExpressionMatch volumeMatch = volumeExpression.match(text);
                       if (volumeMatch.hasMatch()) {
                           m_state.volume = qBound(0,
                                                   qRound(volumeMatch.captured(1).toDouble() * 100.0),
                                                   100);
                       } else if (match.hasMatch()) {
                           m_state.volume = match.captured(1).toInt();
                       }
                       m_state.muted = text.contains(QStringLiteral("[MUTED]"));
                   }

                   m_state.outputDevice = QStringLiteral("Default output");
                   m_state.inputDevice = QStringLiteral("Default input");

                   if (m_devices.isEmpty()) {
                       AudioDeviceInfo output;
                       output.id = QStringLiteral("@DEFAULT_AUDIO_SINK@");
                       output.name = QStringLiteral("Default output");
                       output.isDefault = true;
                       AudioDeviceInfo input;
                       input.id = QStringLiteral("@DEFAULT_AUDIO_SOURCE@");
                       input.name = QStringLiteral("Default input");
                       input.isInput = true;
                       input.isDefault = true;
                       m_devices = {output, input};
                       emit devicesChanged();
                   }
                   finishStep();
               });
}

void LinuxAudioManager::refreshPactl()
{
    m_pendingSteps = 3;

    runCommand(m_program, {QStringLiteral("get-sink-volume"), QStringLiteral("@DEFAULT_SINK@")},
               [this](const QByteArray& output, const QString& error) {
                   if (!error.isEmpty()) {
                       setLastError(error);
                   } else {
                       const QRegularExpressionMatch match = percentExpression().match(QString::fromLocal8Bit(output));
                       if (match.hasMatch()) {
                           m_state.volume = match.captured(1).toInt();
                       }
                   }
                   finishStep();
               });

    runCommand(m_program, {QStringLiteral("get-sink-mute"), QStringLiteral("@DEFAULT_SINK@")},
               [this](const QByteArray& output, const QString&) {
                   m_state.muted = QString::fromLocal8Bit(output).contains(QStringLiteral("yes"));
                   finishStep();
               });

    readPactlDevices();
}

void LinuxAudioManager::readPactlDevices()
{
    runCommand(m_program, {QStringLiteral("list"), QStringLiteral("short"), QStringLiteral("sinks")},
               [this](const QByteArray& sinks, const QString&) {
                   runCommand(m_program, {QStringLiteral("list"), QStringLiteral("short"), QStringLiteral("sources")},
                              [this, sinks](const QByteArray& sources, const QString&) {
                                  QList<AudioDeviceInfo> devices;
                                  const QStringList sinkLines = QString::fromLocal8Bit(sinks).split('\n', Qt::SkipEmptyParts);
                                  for (const QString& line : sinkLines) {
                                      const QStringList fields = line.split('\t');
                                      if (fields.size() < 2) {
                                          continue;
                                      }
                                      AudioDeviceInfo device;
                                      device.id = fields.at(1);
                                      device.name = fields.at(1);
                                      device.name.replace(QLatin1Char('.'), QLatin1Char(' '));
                                      devices.append(device);
                                  }
                                  const QStringList sourceLines = QString::fromLocal8Bit(sources).split('\n', Qt::SkipEmptyParts);
                                  for (const QString& line : sourceLines) {
                                      const QStringList fields = line.split('\t');
                                      if (fields.size() < 2 || fields.at(1).contains(QStringLiteral(".monitor"))) {
                                          continue;
                                      }
                                      AudioDeviceInfo device;
                                      device.id = fields.at(1);
                                      device.name = fields.at(1);
                                      device.name.replace(QLatin1Char('.'), QLatin1Char(' '));
                                      device.isInput = true;
                                      devices.append(device);
                                  }
                                  m_devices = devices;
                                  emit devicesChanged();
                                  finishStep();
                              });
               });
}

void LinuxAudioManager::finishStep()
{
    if (m_pendingSteps <= 0) {
        return;
    }
    if (--m_pendingSteps > 0) {
        return;
    }
    m_refreshing = false;
    emit stateChanged(m_state);
}

bool LinuxAudioManager::setVolume(int percent)
{
    if (m_backend == Backend::None) {
        return false;
    }

    const int clamped = std::clamp(percent, 0, 100);
    QStringList arguments;
    if (m_backend == Backend::Wpctl) {
        arguments = {QStringLiteral("set-volume"), QStringLiteral("@DEFAULT_AUDIO_SINK@"),
                     QStringLiteral("%1%").arg(clamped)};
    } else {
        arguments = {QStringLiteral("set-sink-volume"), QStringLiteral("@DEFAULT_SINK@"),
                     QStringLiteral("%1%").arg(clamped)};
    }

    runCommand(m_program, arguments, [this](const QByteArray&, const QString& error) {
        if (!error.isEmpty()) {
            reportError(error);
        }
    });

    m_state.volume = clamped;
    if (clamped > 0) {
        m_state.muted = false;
    }
    emit stateChanged(m_state);
    return true;
}

bool LinuxAudioManager::setMuted(bool muted)
{
    if (m_backend == Backend::None) {
        return false;
    }

    QStringList arguments;
    if (m_backend == Backend::Wpctl) {
        arguments = {QStringLiteral("set-mute"), QStringLiteral("@DEFAULT_AUDIO_SINK@"),
                     muted ? QStringLiteral("1") : QStringLiteral("0")};
    } else {
        arguments = {QStringLiteral("set-sink-mute"), QStringLiteral("@DEFAULT_SINK@"),
                     muted ? QStringLiteral("1") : QStringLiteral("0")};
    }

    runCommand(m_program, arguments, [this](const QByteArray&, const QString& error) {
        if (!error.isEmpty()) {
            reportError(error);
        }
    });

    m_state.muted = muted;
    emit stateChanged(m_state);
    return true;
}

bool LinuxAudioManager::setDefaultDevice(const QString& id, bool input)
{
    if (m_backend == Backend::None || id.isEmpty()) {
        return false;
    }

    QStringList arguments;
    if (m_backend == Backend::Wpctl) {
        if (id.startsWith(QLatin1Char('@'))) {
            return false;
        }
        arguments = {QStringLiteral("set-default"), id};
    } else {
        arguments = {input ? QStringLiteral("set-default-source") : QStringLiteral("set-default-sink"), id};
    }

    runCommand(m_program, arguments, [this](const QByteArray&, const QString& error) {
        if (!error.isEmpty()) {
            reportError(error);
        }
    });
    return true;
}

} // namespace Aero::Platform::Linux
