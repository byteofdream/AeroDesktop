#include "linux_display_manager.h"

#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QProcess>
#include <QScreen>
#include <QStandardPaths>

namespace Aero::Platform::Linux {
namespace {
QString readValue(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    return QString::fromLocal8Bit(file.readLine()).trimmed();
}
} // namespace

LinuxDisplayManager::LinuxDisplayManager(QObject* parent)
    : IDisplayManager(parent)
{
    m_brightnessctlPath = QStandardPaths::findExecutable(QStringLiteral("brightnessctl"));
    locateBacklight();
    refresh();
}

bool LinuxDisplayManager::isAvailable() const
{
    return true;
}

void LinuxDisplayManager::locateBacklight()
{
    const QDir backlight(QStringLiteral("/sys/class/backlight"));
    const QStringList entries = backlight.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString& entry : entries) {
        const QString base = backlight.filePath(entry);
        bool ok = false;
        const int max = readValue(base + QStringLiteral("/max_brightness")).toInt(&ok);
        if (ok && max > 0) {
            m_backlightPath = base;
            m_backlightMax = max;
            return;
        }
    }
}

QList<DisplayInfo> LinuxDisplayManager::displays() const
{
    return m_displays;
}

void LinuxDisplayManager::refresh()
{
    QList<DisplayInfo> displays;
    const QList<QScreen*> screens = QGuiApplication::screens();
    for (QScreen* screen : screens) {
        if (screen == nullptr) {
            continue;
        }
        DisplayInfo info;
        info.id = screen->name().isEmpty() ? screen->model() : screen->name();
        info.name = screen->model().isEmpty() ? info.id : screen->model();
        info.geometry = screen->geometry();
        info.devicePixelRatio = screen->devicePixelRatio();
        info.refreshRate = screen->refreshRate();
        info.primary = screen == QGuiApplication::primaryScreen();
        info.builtIn = info.id.startsWith(QStringLiteral("eDP"), Qt::CaseInsensitive)
            || info.id.startsWith(QStringLiteral("LVDS"), Qt::CaseInsensitive);
        info.brightness = brightness(info.id);
        displays.append(info);
    }

    m_displays = displays;
    emit displaysChanged();
    for (const DisplayInfo& display : std::as_const(m_displays)) {
        if (display.brightness >= 0) {
            emit brightnessChanged(display.id, display.brightness);
        }
    }
}

int LinuxDisplayManager::brightness(const QString& displayId) const
{
    (void)displayId;
    if (m_backlightPath.isEmpty() || m_backlightMax <= 0) {
        return -1;
    }
    bool ok = false;
    const int value = readValue(m_backlightPath + QStringLiteral("/brightness")).toInt(&ok);
    if (!ok) {
        return -1;
    }
    return qBound(1, qRound(100.0 * value / m_backlightMax), 100);
}

bool LinuxDisplayManager::supportsBrightness() const
{
    return !m_backlightPath.isEmpty() || !m_brightnessctlPath.isEmpty();
}

bool LinuxDisplayManager::setBrightness(const QString& displayId, int percent)
{
    const int clamped = qBound(1, percent, 100);

    if (!m_brightnessctlPath.isEmpty()) {
        auto* process = new QProcess(this);
        process->setProgram(m_brightnessctlPath);
        process->setArguments({QStringLiteral("set"), QStringLiteral("%1%").arg(clamped)});
        connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
                [this, process, clamped, displayId](int, QProcess::ExitStatus status) {
                    if (status != QProcess::NormalExit) {
                        reportError(QString::fromLocal8Bit(process->readAllStandardError()).trimmed());
                    } else {
                        emit brightnessChanged(displayId, clamped);
                    }
                    process->deleteLater();
                });
        process->start();
        return true;
    }

    if (m_backlightPath.isEmpty() || m_backlightMax <= 0) {
        reportError(QStringLiteral("This system does not expose display brightness control"));
        return false;
    }

    QFile file(m_backlightPath + QStringLiteral("/brightness"));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        reportError(QStringLiteral("Unable to adjust brightness: %1").arg(file.errorString()));
        return false;
    }

    const int raw = qBound(0, qRound(m_backlightMax * clamped / 100.0), m_backlightMax);
    file.write(QByteArray::number(raw));
    file.close();

    for (DisplayInfo& display : m_displays) {
        display.brightness = clamped;
    }
    emit brightnessChanged(displayId, clamped);
    return true;
}

} // namespace Aero::Platform::Linux
