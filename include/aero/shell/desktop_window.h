#pragma once

#include <QList>
#include <QPointer>
#include <QRect>
#include <QWidget>

class QLabel;
class QListWidget;
class QTimer;

namespace Aero::Compositor {
class WindowManager;
}
namespace Aero::Platform {
class Platform;
}
namespace Aero::Services {
class NotificationBus;
}
namespace Aero::Settings {
class SettingsManager;
}

namespace Aero::Shell {

class Dock;
class TopBar;
class WallpaperView;

enum class OverlayPlacement {
    Center,
    TopRight,
    TopRightOffset,
    BottomCenter,
    FullScreen,
};

/// Root window of the Aero shell.
///
/// Layer order, from back to front:
///   wallpaper -> desktop icons/window surfaces -> top bar -> dock -> overlays.
///
/// The window manager positions application windows inside workArea(), which
/// excludes the top bar and the dock (unless the dock is configured to hide).
/// Overlays are plain children of this window and are positioned here so a
/// resize, display change or dock move keeps them correctly anchored.
class DesktopWindow : public QWidget {
    Q_OBJECT
public:
    DesktopWindow(Platform::Platform* platform,
                  Settings::SettingsManager* settings,
                  Services::NotificationBus* notifications,
                  QWidget* parent = nullptr);
    ~DesktopWindow() override;

    Compositor::WindowManager* windowManager() const;
    QWidget* workspaceHost() const;

    QRect workArea() const;
    QRect topBarRect() const;
    QRect dockRect() const;

    TopBar* topBar() const;
    Dock* dock() const;

    void addOverlay(QWidget* overlay, OverlayPlacement placement);
    void showOverlay(QWidget* overlay);
    void hideOverlays(QWidget* except = nullptr);
    bool isOverlayVisible(QWidget* overlay) const;

    /// Re-reads appearance, wallpaper and dock settings.
    void applySettings();

    /// Rebuilds the cached desktop backdrop used by glass panels.  The capture
    /// deliberately excludes the top bar, dock and every overlay.
    void refreshBackdrop();

    void refreshDesktopIcons();

signals:
    void openPathRequested(const QString& path);
    void desktopContextMenuRequested(const QPoint& globalPosition);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    struct OverlayEntry {
        QPointer<QWidget> widget;
        OverlayPlacement placement = OverlayPlacement::Center;
    };

    void buildUi();
    void buildDesktopIcons();
    void layoutChildren();
    void positionOverlays();
    void updateWorkArea();
    void updateGlassBackdrops(const QPixmap& backdrop);
    void scheduleBackdrop();
    void hideDock();
    void revealDock();
    void scheduleDockHide();
    void updateDockGeometry();
    void updateDockMode();
    void handleDockEdgePointer(const QPoint& globalPosition);

    Platform::Platform* m_platform = nullptr;
    Settings::SettingsManager* m_settings = nullptr;
    Services::NotificationBus* m_notifications = nullptr;

    WallpaperView* m_wallpaper = nullptr;
    QWidget* m_workspaceHost = nullptr;
    QListWidget* m_desktopIcons = nullptr;
    TopBar* m_topBar = nullptr;
    Dock* m_dock = nullptr;
    Compositor::WindowManager* m_windowManager = nullptr;

    QList<OverlayEntry> m_overlays;
    QTimer* m_backdropTimer = nullptr;
    QTimer* m_dockHideTimer = nullptr;
    bool m_dockConcealed = false;
    bool m_firstShow = true;
    qint64 m_lastDesktopOpenMs = 0;
};

} // namespace Aero::Shell
