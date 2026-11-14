#include "aero/graphics/glass_panel.h"

#include "aero/core/utils.h"
#include "aero/theme/theme.h"

#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

namespace Aero::Graphics {

GlassPanel::GlassPanel(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAutoFillBackground(false);
}

void GlassPanel::setCornerRadius(int radius)
{
    m_radius = qMax(0, radius);
    update();
}

int GlassPanel::cornerRadius() const
{
    return m_radius;
}

void GlassPanel::setTintStrength(qreal strength)
{
    m_tintStrength = Core::Utils::clamp(strength, 0.0, 2.0);
    update();
}

qreal GlassPanel::tintStrength() const
{
    return m_tintStrength;
}

void GlassPanel::setBackdrop(const QPixmap& backdrop, const QPoint& globalOrigin)
{
    m_backdrop = backdrop;
    m_backdropOrigin = globalOrigin;
    update();
}

void GlassPanel::clearBackdrop()
{
    m_backdrop = {};
    update();
}

bool GlassPanel::hasBackdrop() const
{
    return !m_backdrop.isNull();
}

void GlassPanel::setBorderVisible(bool visible)
{
    m_borderVisible = visible;
    update();
}

void GlassPanel::setHighlightVisible(bool visible)
{
    m_highlightVisible = visible;
    update();
}

void GlassPanel::setShadowMargin(int margin)
{
    m_shadowMargin = qMax(0, margin);
    update();
}

int GlassPanel::shadowMargin() const
{
    return m_shadowMargin;
}

QRectF GlassPanel::surfaceRect() const
{
    return QRectF(rect()).adjusted(m_shadowMargin, m_shadowMargin, -m_shadowMargin, -m_shadowMargin);
}

void GlassPanel::paintShadow(QPainter& painter, const QRectF& bounds) const
{
    const Theme::ThemeTokens& tokens = Theme::ThemeManager::instance().tokens();
    const int layers = 10;
    for (int i = layers; i >= 1; --i) {
        const qreal t = static_cast<qreal>(i) / layers;
        const qreal spread = 2.0 + t * 16.0;
        QColor color = tokens.color.shadow;
        color.setAlphaF(color.alphaF() * (1.0 - t) * 0.55);
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        painter.drawRoundedRect(bounds.adjusted(-spread, -spread * 0.6, spread, spread * 1.1),
                                m_radius + spread,
                                m_radius + spread);
    }
}

void GlassPanel::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    const Theme::ThemeTokens& tokens = Theme::ThemeManager::instance().tokens();
    const Theme::ColorTokens& colors = tokens.color;
    const QRectF surface = surfaceRect();
    if (surface.width() <= 1.0 || surface.height() <= 1.0) {
        return;
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    if (m_shadowMargin > 0) {
        paintShadow(painter, surface);
    }

    QPainterPath clip;
    clip.addRoundedRect(surface, m_radius, m_radius);

    painter.save();
    painter.setClipPath(clip);

    if (!m_backdrop.isNull()) {
        const QPoint localOrigin = mapToGlobal(QPoint(0, 0)) - m_backdropOrigin;
        const QRect sourceRect(localOrigin + QPoint(m_shadowMargin, m_shadowMargin),
                               surface.size().toSize());
        painter.drawPixmap(QRectF(surface.topLeft(), surface.size()), m_backdrop, QRectF(sourceRect));
    }

    QColor topTint = colors.glassTintStrong;
    QColor bottomTint = colors.glassTint;
    topTint.setAlphaF(Core::Utils::clamp(topTint.alphaF() * m_tintStrength, 0.0, 1.0));
    bottomTint.setAlphaF(Core::Utils::clamp(bottomTint.alphaF() * m_tintStrength, 0.0, 1.0));

    QLinearGradient gradient(surface.topLeft(), surface.bottomLeft());
    gradient.setColorAt(0.0, topTint);
    gradient.setColorAt(0.55, bottomTint);
    gradient.setColorAt(1.0, bottomTint);
    painter.fillRect(surface, gradient);

    if (m_highlightVisible) {
        QLinearGradient sheen(surface.topLeft(), QPointF(surface.left(), surface.top() + surface.height() * 0.45));
        sheen.setColorAt(0.0, Core::Utils::withAlpha(colors.glassHighlight, 0.34));
        sheen.setColorAt(1.0, QColor(255, 255, 255, 0));
        painter.fillRect(surface, sheen);
    }

    if (m_pressed) {
        painter.fillRect(surface, Core::Utils::withAlpha(colors.textPrimary, 0.035));
    }
    painter.restore();

    if (m_borderVisible) {
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(Core::Utils::withAlpha(colors.glassBorder, 0.85), 1.0));
        painter.drawRoundedRect(surface.adjusted(0.5, 0.5, -0.5, -0.5), m_radius, m_radius);
    }
}

void GlassPanel::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_pressed = false;
        update();
        if (rect().contains(event->position().toPoint())) {
            emit clicked();
        }
    }
    QWidget::mouseReleaseEvent(event);
}

} // namespace Aero::Graphics
