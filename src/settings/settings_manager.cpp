#include "aero/settings/settings_manager.h"

#include "aero/core/json_config.h"
#include "aero/core/logging.h"
#include "aero/core/paths.h"
#include "aero/settings/settings_keys.h"

#include <QColor>
#include <QDir>

namespace Aero::Settings {
namespace {

const QStringList kDefaultPinned = {
    QStringLiteral("aero-finder"),
    QStringLiteral("aero-terminal"),
    QStringLiteral("aero-editor"),
    QStringLiteral("aero-calendar"),
    QStringLiteral("aero-monitor"),
    QStringLiteral("aero-calculator"),
    QStringLiteral("aero-tasks"),
    QStringLiteral("aero-archive"),
    QStringLiteral("aero-settings"),
    QStringLiteral("aero-about"),
};

const QStringList kAppearanceKeys = {
    QString::fromLatin1(Keys::AppearanceMode),
    QString::fromLatin1(Keys::AccentColor),
    QString::fromLatin1(Keys::FontScale),
    QString::fromLatin1(Keys::ReducedMotion),
    QString::fromLatin1(Keys::Transparency),
    QString::fromLatin1(Keys::Blur),
    QString::fromLatin1(Keys::HighContrast),
};

bool isAppearanceKey(const QString& key)
{
    return kAppearanceKeys.contains(key);
}

bool isDockKey(const QString& key)
{
    return key.startsWith(QStringLiteral("dock."));
}

} // namespace

SettingsManager::SettingsManager(QObject* parent)
    : SettingsManager(Core::Paths::settingsFile(), parent)
{
}

SettingsManager::SettingsManager(const QString& filePath, QObject* parent)
    : QObject(parent)
    , m_config(std::make_unique<Core::JsonConfig>(filePath))
{
    connectConfig();
    QString error;
    if (!load(&error)) {
        qCWarning(aeroLog) << "Settings load failed:" << error;
    }
}

SettingsManager::~SettingsManager()
{
    if (m_config) {
        m_config->flush();
    }
}

void SettingsManager::connectConfig()
{
    connect(m_config.get(), &Core::JsonConfig::changed, this,
            [this](const QString& key, const QVariant& value) {
                notify(key);
                emit settingChanged(key, value);
            });
}

bool SettingsManager::load(QString* error)
{
    const bool ok = m_config->load(error);
    seedDefaults();
    return ok;
}

bool SettingsManager::save(QString* error)
{
    return m_config->save(error);
}

void SettingsManager::flush()
{
    m_config->flush();
}

QString SettingsManager::filePath() const
{
    return m_config->filePath();
}

void SettingsManager::seedDefaults()
{
    // Defaults are read lazily; only values that must exist for other modules
    // (pinned dock applications) are seeded so user files stay small.
    if (!m_config->contains(QString::fromLatin1(Keys::DockPinnedApps))) {
        m_config->setValue(QString::fromLatin1(Keys::DockPinnedApps), kDefaultPinned);
    }
    if (!m_config->contains(QString::fromLatin1(Keys::AppearanceMode))) {
        m_config->setValue(QString::fromLatin1(Keys::AppearanceMode), QStringLiteral("auto"));
    }
    if (!m_config->contains(QString::fromLatin1(Keys::AccentColor))) {
        m_config->setValue(QString::fromLatin1(Keys::AccentColor), QStringLiteral("#7C5CFF"));
    }
}

QVariant SettingsManager::value(const QString& key, const QVariant& fallback) const
{
    return m_config->value(key, fallback);
}

void SettingsManager::setValue(const QString& key, const QVariant& value)
{
    m_config->setValue(key, value);
}

void SettingsManager::resetToDefaults()
{
    m_config->replaceRoot({});
    seedDefaults();
    notify(QString());
    emit settingChanged(QString(), {});
}

void SettingsManager::notify(const QString& key)
{
    if (key.isEmpty() || isAppearanceKey(key)) {
        emit appearanceChanged();
    }
    if (key.isEmpty() || isDockKey(key)) {
        emit dockChanged();
    }
    if (key.isEmpty() || key.startsWith(QStringLiteral("shortcuts."))) {
        emit shortcutsChanged();
    }
}

QString SettingsManager::appearanceMode() const
{
    return m_config->stringValue(QString::fromLatin1(Keys::AppearanceMode), QStringLiteral("auto"));
}

void SettingsManager::setAppearanceMode(const QString& mode)
{
    m_config->setValue(QString::fromLatin1(Keys::AppearanceMode), mode);
}

QString SettingsManager::accentColor() const
{
    return m_config->stringValue(QString::fromLatin1(Keys::AccentColor), QStringLiteral("#7C5CFF"));
}

void SettingsManager::setAccentColor(const QString& color)
{
    m_config->setValue(QString::fromLatin1(Keys::AccentColor), color);
}

qreal SettingsManager::fontScale() const
{
    return m_config->doubleValue(QString::fromLatin1(Keys::FontScale), 1.0);
}

void SettingsManager::setFontScale(qreal scale)
{
    m_config->setValue(QString::fromLatin1(Keys::FontScale), scale);
}

bool SettingsManager::reducedMotion() const
{
    return m_config->boolValue(QString::fromLatin1(Keys::ReducedMotion), false);
}

void SettingsManager::setReducedMotion(bool reduced)
{
    m_config->setValue(QString::fromLatin1(Keys::ReducedMotion), reduced);
}

bool SettingsManager::transparencyEnabled() const
{
    return m_config->boolValue(QString::fromLatin1(Keys::Transparency), true);
}

void SettingsManager::setTransparencyEnabled(bool enabled)
{
    m_config->setValue(QString::fromLatin1(Keys::Transparency), enabled);
}

bool SettingsManager::blurEnabled() const
{
    return m_config->boolValue(QString::fromLatin1(Keys::Blur), true);
}

void SettingsManager::setBlurEnabled(bool enabled)
{
    m_config->setValue(QString::fromLatin1(Keys::Blur), enabled);
}

QString SettingsManager::wallpaperId() const
{
    return m_config->stringValue(QString::fromLatin1(Keys::WallpaperId), QStringLiteral("aurora"));
}

QString SettingsManager::wallpaperPath() const
{
    return m_config->stringValue(QString::fromLatin1(Keys::WallpaperPath));
}

QString SettingsManager::wallpaperFit() const
{
    return m_config->stringValue(QString::fromLatin1(Keys::WallpaperFit), QStringLiteral("fill"));
}

void SettingsManager::setWallpaper(const QString& id, const QString& path, const QString& fit)
{
    m_config->setValue(QString::fromLatin1(Keys::WallpaperId), id);
    m_config->setValue(QString::fromLatin1(Keys::WallpaperPath), path);
    m_config->setValue(QString::fromLatin1(Keys::WallpaperFit), fit);
}

QString SettingsManager::dockPosition() const
{
    return m_config->stringValue(QString::fromLatin1(Keys::DockPosition), QStringLiteral("bottom"));
}

int SettingsManager::dockIconSize() const
{
    return qBound(32, m_config->intValue(QString::fromLatin1(Keys::DockIconSize), 48), 80);
}

qreal SettingsManager::dockMagnification() const
{
    return qBound(1.0, m_config->doubleValue(QString::fromLatin1(Keys::DockMagnification), 1.3), 2.0);
}

bool SettingsManager::dockAutoHide() const
{
    return m_config->boolValue(QString::fromLatin1(Keys::DockAutoHide), false);
}

QStringList SettingsManager::pinnedApplications() const
{
    return m_config->stringListValue(QString::fromLatin1(Keys::DockPinnedApps), kDefaultPinned);
}

void SettingsManager::setPinnedApplications(const QStringList& appIds)
{
    m_config->setValue(QString::fromLatin1(Keys::DockPinnedApps), appIds);
}

int SettingsManager::displayBrightness() const
{
    return qBound(5, m_config->intValue(QString::fromLatin1(Keys::DisplayBrightness), 80), 100);
}

int SettingsManager::soundVolume() const
{
    return qBound(0, m_config->intValue(QString::fromLatin1(Keys::SoundVolume), 60), 100);
}

bool SettingsManager::soundMuted() const
{
    return m_config->boolValue(QString::fromLatin1(Keys::SoundMuted), false);
}

bool SettingsManager::doNotDisturb() const
{
    return m_config->boolValue(QString::fromLatin1(Keys::NotificationsDnd), false);
}

void SettingsManager::setDoNotDisturb(bool enabled)
{
    m_config->setValue(QString::fromLatin1(Keys::NotificationsDnd), enabled);
}

QStringList SettingsManager::recentFiles() const
{
    return m_config->stringListValue(QStringLiteral("recent.files"), {});
}

void SettingsManager::addRecentFile(const QString& path)
{
    if (path.isEmpty()) {
        return;
    }
    QStringList recent = recentFiles();
    recent.removeAll(path);
    recent.prepend(path);
    while (recent.size() > 12) {
        recent.removeLast();
    }
    m_config->setValue(QStringLiteral("recent.files"), recent);
}

void SettingsManager::clearRecentFiles()
{
    m_config->remove(QStringLiteral("recent.files"));
}

QKeySequence SettingsManager::shortcut(const QString& action) const
{
    const QHash<QString, QKeySequence> defaults = defaultShortcuts();
    const QString value = m_config->stringValue(QString::fromLatin1(Keys::ShortcutsPrefix) + action);
    if (value.isEmpty()) {
        return defaults.value(action);
    }
    return QKeySequence::fromString(value, QKeySequence::PortableText);
}

void SettingsManager::setShortcut(const QString& action, const QKeySequence& sequence)
{
    m_config->setValue(QString::fromLatin1(Keys::ShortcutsPrefix) + action,
                       sequence.toString(QKeySequence::PortableText));
}

QHash<QString, QKeySequence> SettingsManager::shortcuts() const
{
    QHash<QString, QKeySequence> result = defaultShortcuts();
    for (auto it = result.cbegin(); it != result.cend(); ++it) {
        const QString value = m_config->stringValue(QString::fromLatin1(Keys::ShortcutsPrefix) + it.key());
        if (!value.isEmpty()) {
            result[it.key()] = QKeySequence::fromString(value, QKeySequence::PortableText);
        }
    }
    return result;
}

QHash<QString, QKeySequence> SettingsManager::defaultShortcuts()
{
    return {
        {QStringLiteral("spotlight"), QKeySequence(QStringLiteral("Meta+Space"))},
        {QStringLiteral("control-center"), QKeySequence(QStringLiteral("Meta+C"))},
        {QStringLiteral("notification-center"), QKeySequence(QStringLiteral("Meta+N"))},
        {QStringLiteral("workspace-overview"), QKeySequence(QStringLiteral("Meta+Tab"))},
        {QStringLiteral("workspace-next"), QKeySequence(QStringLiteral("Ctrl+Alt+Right"))},
        {QStringLiteral("workspace-previous"), QKeySequence(QStringLiteral("Ctrl+Alt+Left"))},
        {QStringLiteral("window-close"), QKeySequence(QStringLiteral("Alt+F4"))},
        {QStringLiteral("window-minimize"), QKeySequence(QStringLiteral("Meta+M"))},
        {QStringLiteral("window-maximize"), QKeySequence(QStringLiteral("Meta+Up"))},
        {QStringLiteral("window-switch"), QKeySequence(QStringLiteral("Alt+Tab"))},
        {QStringLiteral("show-desktop"), QKeySequence(QStringLiteral("Meta+D"))},
        {QStringLiteral("lock-screen"), QKeySequence(QStringLiteral("Meta+L"))},
    };
}

} // namespace Aero::Settings
