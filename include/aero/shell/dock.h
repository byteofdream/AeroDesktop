#pragma once

#include "aero/graphics/glass_panel.h"

#include <QElapsedTimer>
#include <QPoint>
#include <QPointF>
#include <QSize>
#include <QStringList>

class QTimer;

namespace Aero::Compositor {
class WindowManager;
}
namespace Aero::Settings {
class SettingsManager;
}

namespace Aero::Shell {

/// Floating application dock with magnification, running indicators and a
/// launch bounce.  It paints every icon itself so magnification remains smooth
/// and does not allocate a widget per frame.
class Dock : public Graphics::GlassPanel {
    Q_OBJECT
public:
    Dock(Settings::SettingsManager* settings,
         Compositor::WindowManager* windowManager,
         QWidget* parent = nullptr);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    void refresh();
    void setOrientation(Qt::Orientation orientation);
    Qt::Orientation orientation() const;

    int preferredThickness() const;
    QSize preferredSize() const;

    void setConcealed(bool concealed);
    bool isConcealed() const;

    void bounce(const QString& appId);

signals:
    void appActivated(const QString& appId);
    void appMenuRequested(const QString& appId, const QPoint& globalPosition);
    void dockMenuRequested(const QPoint& globalPosition);
    void preferredSizeChanged();

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    int iconSize() const;
    int itemGap() const;
    int surfacePadding() const;
    QRectF itemRect(int index) const;
    QRectF iconRect(int index) const;
    int indexAt(const QPointF& position) const;
    void rebuildItems();
    void requestRepaint();
    void startAnimation();
    void stopAnimation();
    void updateBounce();

    Settings::SettingsManager* m_settings = nullptr;
    Compositor::WindowManager* m_windowManager = nullptr;
    QStringList m_apps;
    QList<bool> m_running;
    Qt::Orientation m_orientation = Qt::Horizontal;
    QPointF m_pointer;
    int m_hovered = -1;
    bool m_inside = false;
    bool m_concealed = false;

    QString m_bounceApp;
    QElapsedTimer m_bounceTimer;
    QTimer* m_animationTimer = nullptr;
    int m_animationTicks = 0;
};

} // namespace Aero::Shell
