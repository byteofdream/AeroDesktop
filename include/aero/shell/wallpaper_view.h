#pragma once

#include <QPixmap>
#include <QSize>
#include <QString>
#include <QWidget>

namespace Aero::Settings {
class SettingsManager;
}

namespace Aero::Shell {

/// Paints the configured Aero wallpaper.
///
/// Built-in wallpapers are vector gradients shared with Settings.  A user
/// supplied image is loaded through QImageReader and cached at the current
/// desktop size so scrolling, window animations and glass sampling never
/// re-decode it.  If the image cannot be read, the class falls back to the
/// configured preset instead of painting an empty desktop.
class WallpaperView : public QWidget {
    Q_OBJECT
public:
    explicit WallpaperView(Settings::SettingsManager* settings, QWidget* parent = nullptr);

    void reload();
    QString currentWallpaperId() const;

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void rebuildCache();
    void rebuildDesktopTexture();

    Settings::SettingsManager* m_settings = nullptr;
    QPixmap m_cache;
    QPixmap m_texture;
    QSize m_cacheSize;
    QString m_cachedId;
    QString m_cachedPath;
    QString m_cachedFit;
    bool m_cacheValid = false;
};

} // namespace Aero::Shell
