#pragma once

#include <QWidget>

class QPainter;

namespace Aero::Graphics {

/// A translucent, rounded surface that samples a pre-blurred desktop snapshot.
///
/// The widget is transparent by default, so it composes correctly over the
/// wallpaper, application windows and each other.  Call setBackdrop() with the
/// snapshot produced by BackdropProvider to enable real background sampling;
/// without it the panel paints a tuned translucent tint and remains readable.
class GlassPanel : public QWidget {
    Q_OBJECT
public:
    explicit GlassPanel(QWidget* parent = nullptr);

    void setCornerRadius(int radius);
    int cornerRadius() const;

    void setTintStrength(qreal strength);
    qreal tintStrength() const;

    void setBackdrop(const QPixmap& backdrop, const QPoint& globalOrigin = {});
    void clearBackdrop();
    bool hasBackdrop() const;

    void setBorderVisible(bool visible);
    void setHighlightVisible(bool visible);
    void setShadowMargin(int margin);
    int shadowMargin() const;

    /// The rectangle inside the shadow margin, in local coordinates.
    QRectF surfaceRect() const;

signals:
    void clicked();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    void paintShadow(QPainter& painter, const QRectF& rect) const;

    int m_radius = 18;
    qreal m_tintStrength = 1.0;
    QPixmap m_backdrop;
    QPoint m_backdropOrigin;
    bool m_borderVisible = true;
    bool m_highlightVisible = true;
    bool m_pressed = false;
    int m_shadowMargin = 0;
};

} // namespace Aero::Graphics
