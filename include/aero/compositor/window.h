#pragma once

#include <QIcon>
#include <QPointer>
#include <QRect>
#include <QSize>
#include <QWidget>

#include <functional>

class QGraphicsOpacityEffect;

namespace Aero::Compositor {

struct WindowOptions {
    QString applicationId;
    QString title;
    QString subtitle;
    QIcon icon;
    QWidget* content = nullptr; // ownership transferred to the window
    QSize defaultSize {900, 600};
    QSize minimumSize {420, 280};
    bool resizable = true;
    bool minimizable = true;
    bool maximizable = true;
    bool closable = true;
    bool startMaximized = false;
    std::function<bool()> closeValidator;
};

/// Decorated, animated application window surface.
///
/// AeroWindow implements the interactive parts of window management (drag,
/// eight-direction resize, traffic-light controls, titlebar menu, shadow and
/// rounded glass frame).  WindowManager owns placement, stacking, focus,
/// workspaces and lifecycle.
class AeroWindow : public QWidget {
    Q_OBJECT
    Q_PROPERTY(QRect surfaceGeometry READ surfaceGeometry WRITE setSurfaceGeometry)

public:
    enum class State {
        Normal,
        Minimized,
        Maximized,
        Fullscreen,
    };

    explicit AeroWindow(int id, WindowOptions options, QWidget* parent = nullptr);
    ~AeroWindow() override;

    int id() const;
    QString applicationId() const;
    QString title() const;
    void setTitle(const QString& title);
    QString subtitle() const;
    void setSubtitle(const QString& subtitle);

    QWidget* contentWidget() const;

    State state() const;
    void setState(State state);

    int workspace() const;
    void setWorkspace(int workspace);

    bool isActive() const;
    void setActive(bool active);

    QRect surfaceGeometry() const;
    void setSurfaceGeometry(const QRect& surface);
    QRect normalSurfaceGeometry() const;
    void setNormalSurfaceGeometry(const QRect& geometry);

    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;

    bool canClose() const;

    void setOpacity(qreal opacity);
    qreal opacity() const;

    QPixmap thumbnail(const QSize& size);

signals:
    void closeRequested(int id);
    void minimizeRequested(int id);
    void maximizeRequested(int id, bool maximized);
    void activated(int id);
    void dragFinished(int id);
    void geometryCommitted(int id, const QRect& surfaceGeometry);
    void contextMenuRequested(int id, const QPoint& globalPosition);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void moveEvent(QMoveEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void enterEvent(QEnterEvent* event) override;

private:
    enum class HoverButton {
        None,
        Close,
        Minimize,
        Maximize,
    };

    enum ResizeEdge {
        NoEdge = 0,
        LeftEdge = 1,
        RightEdge = 2,
        TopEdge = 4,
        BottomEdge = 8,
    };

    int margin() const;
    int titleBarHeight() const;
    QRect titleBarRect() const;
    QRect contentRect() const;
    QRect trafficLightRect(HoverButton button) const;
    HoverButton buttonAt(const QPoint& position) const;
    int resizeEdgesAt(const QPoint& position) const;
    void updateCursorFor(const QPoint& position);
    void updateContentGeometry();
    void updateContentMask();
    QCursor resizeCursor(int edges) const;
    void paintShadow(QPainter& painter, const QRectF& surface);
    void paintTrafficLights(QPainter& painter, const QRectF& surface);
    void paintTitle(QPainter& painter, const QRectF& surface);

    int m_id = 0;
    WindowOptions m_options;
    QPointer<QWidget> m_content;
    State m_state = State::Normal;
    State m_stateBeforeMinimize = State::Normal;
    int m_workspace = 0;
    bool m_active = false;
    bool m_titleBarHovered = false;
    HoverButton m_hoverButton = HoverButton::None;
    QRect m_surfaceGeometry;
    QRect m_normalSurfaceGeometry;

    bool m_dragging = false;
    bool m_resizing = false;
    int m_resizeEdges = NoEdge;
    QPoint m_dragOffset;
    QPoint m_resizeStartGlobal;
    QRect m_resizeStartSurface;
    QRect m_dragStartGeometry;

    QGraphicsOpacityEffect* m_opacityEffect = nullptr;
};

} // namespace Aero::Compositor
