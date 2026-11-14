#pragma once

#include <QKeySequence>
#include <QObject>
#include <QStringList>
#include <QVariant>

#include <memory>

namespace Aero::Core {
class JsonConfig;
}

namespace Aero::Settings {

/// Typed, persistent preference store.  Every write is debounced and committed
/// atomically by JsonConfig.  Settings are independent from the platform
/// backend; shell code observes settingChanged() and applies the relevant
/// values to AeroTheme, AeroCompositor or the platform services.
class SettingsManager : public QObject {
    Q_OBJECT
public:
    explicit SettingsManager(QObject* parent = nullptr);
    explicit SettingsManager(const QString& filePath, QObject* parent = nullptr);
    ~SettingsManager() override;

    bool load(QString* error = nullptr);
    bool save(QString* error = nullptr);
    void flush();
    QString filePath() const;

    QVariant value(const QString& key, const QVariant& fallback = {}) const;
    void setValue(const QString& key, const QVariant& value);
    void resetToDefaults();

    QString appearanceMode() const;
    void setAppearanceMode(const QString& mode);
    QString accentColor() const;
    void setAccentColor(const QString& color);
    qreal fontScale() const;
    void setFontScale(qreal scale);
    bool reducedMotion() const;
    void setReducedMotion(bool reduced);
    bool transparencyEnabled() const;
    void setTransparencyEnabled(bool enabled);
    bool blurEnabled() const;
    void setBlurEnabled(bool enabled);

    QString wallpaperId() const;
    QString wallpaperPath() const;
    QString wallpaperFit() const;
    void setWallpaper(const QString& id, const QString& path, const QString& fit);

    QString dockPosition() const;
    int dockIconSize() const;
    qreal dockMagnification() const;
    bool dockAutoHide() const;
    QStringList pinnedApplications() const;
    void setPinnedApplications(const QStringList& appIds);

    int displayBrightness() const;
    int soundVolume() const;
    bool soundMuted() const;
    bool doNotDisturb() const;
    void setDoNotDisturb(bool enabled);

    QStringList recentFiles() const;
    void addRecentFile(const QString& path);
    void clearRecentFiles();

    QKeySequence shortcut(const QString& action) const;
    void setShortcut(const QString& action, const QKeySequence& sequence);
    QHash<QString, QKeySequence> shortcuts() const;
    static QHash<QString, QKeySequence> defaultShortcuts();

signals:
    void settingChanged(const QString& key, const QVariant& value);
    void appearanceChanged();
    void dockChanged();
    void shortcutsChanged();

private:
    void seedDefaults();
    void connectConfig();
    void notify(const QString& key);

    std::unique_ptr<Core::JsonConfig> m_config;
};
} // namespace Aero::Settings
