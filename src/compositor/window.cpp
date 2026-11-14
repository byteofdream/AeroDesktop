#include "aero/compositor/window.h"

#include "aero/core/utils.h"
#include "aero/theme/icon_factory.h"
#include "aero/theme/theme.h"

#include <QEnterEvent>
#include <QFontMetrics>
#include <QGraphicsOpacityEffect>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRegion>
#include <QResizeEvent>

#include <algorithm>
#include <cmath>

namespace Aero::Compositor {
namespace {
constexpr int kShadowMargin = 20;
constexpr int kTitleBarHeight = 44;
constexpr int kWindowRadius = 16;
constexpr int kResizeBorder = 9;
} // namespace

AeroWindow::AeroWindow(int id, WindowOptions options, QWidget* parent)
    : QWidget(parent)
    , m_id(id)
    , m_options(std::move(options))
{
    setAttribute(Qt::WA_NoSystemBackground, false);
    setMouseTracking(true);
    setFocusPolicy(Qt::NoFocus);

    m_content = m_options.content;
    if (m_content != nullptr) {
        m_content->setParent(this);
        m_content->show();
    }

    m_normalSurfaceGeometry = QRect(QPoint(0, 0), m_options.defaultSize);

    const QSize minSurface = m_options.minimumSize.expandedTo(QSize(280, 180));
    setMinimumSize(minSurface + QSize(2 * kShadowMargin, 2 * kShadowMargin));
    setState(m_options.startMaximized ? State::Maximized : State::Normal);
}

AeroWindow::~AeroWindow() = default;

int AeroWindow::id() const
{
    return m_id;
}

QString AeroWindow::applicationId() const
{
    return m_options.applicationId;
}

QString AeroWindow::title() const
{
    return m_options.title;
}

void AeroWindow::setTitle(const QString& title)
{
    if (m_options.title == title) {
        return;
    }
    m_options.title = title;
    update();
}

QString AeroWindow::subtitle() const
{
    return m_options.subtitle;
}

void AeroWindow::setSubtitle(const QString& subtitle)
{
    if (m_options.subtitle == subtitle) {
        return;
    }
    m_options.subtitle = subtitle;
    update();
}

QWidget* AeroWindow::contentWidget() const
{
    return m_content;
}

AeroWindow::State AeroWindow::state() const
{
    return m_state;
}

void AeroWindow::setState(State state)
{
    if (m_state == state) {
        return;
    }

    m_state = state;
    const QSize minSurface = m_options.minimumSize.expandedTo(QSize(280, 180));
    setMinimumSize(minSurface + QSize(2 * margin(), 2 * margin()));
    updateContentGeometry();
    update();
}

int AeroWindow::workspace() const
{
    return m_workspace;
}

void AeroWindow::setWorkspace(int workspace)
{
    m_workspace = workspace;
}

bool AeroWindow::isActive() const
{
    return m_active;
}

void AeroWindow::setActive(bool active)
{
    if (m_active == active) {
        return;
    }
    m_active = active;
    update();
}

QRect AeroWindow::surfaceGeometry() const
{
    return m_surfaceGeometry;
}

void AeroWindow::setSurfaceGeometry(const QRect& surface)
{
    QRect target = surface;
    const QSize minSurface = m_options.minimumSize.expandedTo(QSize(280, 180));
    target.setWidth(qMax(target.width(), minSurface.width()));
    target.setHeight(qMax(target.height(), minSurface.height()));

    m_surfaceGeometry = target;
    const int m = margin();
    setGeometry(target.x() - m, target.y() - m, target.width() + 2 * m, target.height() + 2 * m);
    updateContentGeometry();
}

QRect AeroWindow::normalSurfaceGeometry() const
{
    return m_normalSurfaceGeometry.isValid() ? m_normalSurfaceGeometry : m_surfaceGeometry;
}

void AeroWindow::setNormalSurfaceGeometry(const QRect& geometry)
{
    m_normalSurfaceGeometry = geometry;
}

QSize AeroWindow::minimumSizeHint() const
{
    return m_options.minimumSize.expandedTo(QSize(280, 180));
}

QSize AeroWindow::sizeHint() const
{
    return m_options.defaultSize;
}

bool AeroWindow::canClose() const
{
    if (!m_options.closeValidator) {
        return true;
    }
    try {
        return m_options.closeValidator();
    } catch (...) {
        return true;
    }
}

void AeroWindow::setOpacity(qreal opacity)
{
    const qreal clamped = Core::Utils::clamp(opacity, 0.0, 1.0);
    if (clamped >= 0.999) {
        if (m_opacityEffect != nullptr) {
            setGraphicsEffect(nullptr); // ownership returns to Qt and the effect is deleted
            m_opacityEffect = nullptr;
        }
        return;
    }

    if (m_opacityEffect == nullptr) {
        m_opacityEffect = new QGraphicsOpacityEffect(this);
        m_opacityEffect->setOpacity(clamped);
        setGraphicsEffect(m_opacityEffect);
    } else {
        m_opacityEffect->setOpacity(clamped);
    }
}

qreal AeroWindow::opacity() const
{
    return m_opacityEffect != nullptr ? m_opacityEffect->opacity() : 1.0;
}

QPixmap AeroWindow::thumbnail(const QSize& size)
{
    const QPixmap shot = grab();
    if (shot.isNull()) {
        return {};
    }
    return shot.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

int AeroWindow::margin() const
{
    return (m_state == State::Maximized || m_state == State::Fullscreen) ? 0 : kShadowMargin;
}

int AeroWindow::titleBarHeight() const
{
    return m_state == State::Fullscreen ? 0 : kTitleBarHeight;
}

QRect AeroWindow::titleBarRect() const
{
    const int m = margin();
    return QRect(m, m, width() - 2 * m, titleBarHeight());
}

QRect AeroWindow::contentRect() const
{
    const int m = margin();
    const int top = m + titleBarHeight() + 1;
    return QRect(m + 1, top, qMax(1, width() - 2 * m - 2), qMax(1, height() - top - m - 1));
}

QRect AeroWindow::trafficLightRect(HoverButton button) const
{
    const int index = button == HoverButton::Close ? 0 : (button == HoverButton::Minimize ? 1 : 2);
    const int m = margin();
    const int cx = m + 21 + index * 22;
    const int cy = m + titleBarHeight() / 2;
    return QRect(cx - 7, cy - 7, 14, 14);
}

AeroWindow::HoverButton AeroWindow::buttonAt(const QPoint& position) const
{
    if (titleBarHeight() == 0) {
        return HoverButton::None;
    }
    for (HoverButton button : {HoverButton::Close, HoverButton::Minimize, HoverButton::Maximize}) {
        if (trafficLightRect(button).adjusted(-3, -3, 3, 3).contains(position)) {
            return button;
        }
    }
    return HoverButton::None;
}

int AeroWindow::resizeEdgesAt(const QPoint& position) const
{
    if (!m_options.resizable || m_state == State::Maximized || m_state == State::Fullscreen) {
        return NoEdge;
    }

    const int m = margin();
    const QRect surface(m, m, width() - 2 * m, height() - 2 * m);
    const QRect outer = surface.adjusted(-m, -m, m, m);
    if (!outer.contains(position)) {
        return NoEdge;
    }
    if (titleBarHeight() > 0 && titleBarRect().contains(position) && buttonAt(position) != HoverButton::None) {
        return NoEdge;
    }

    int edges = NoEdge;
    if (qAbs(position.x() - surface.left()) <= kResizeBorder) {
        edges |= LeftEdge;
    }
    if (qAbs(position.x() - surface.right()) <= kResizeBorder) {
        edges |= RightEdge;
    }
    if (qAbs(position.y() - surface.top()) <= kResizeBorder) {
        edges |= TopEdge;
    }
    if (qAbs(position.y() - surface.bottom()) <= kResizeBorder) {
        edges |= BottomEdge;
    }
    return edges;
}

QCursor AeroWindow::resizeCursor(int edges) const
{
    if ((edges & LeftEdge) && (edges & TopEdge)) {
        return Qt::SizeFDiagCursor;
    }
    if ((edges & RightEdge) && (edges & TopEdge)) {
        return Qt::SizeBDiagCursor;
    }
    if ((edges & LeftEdge) && (edges & BottomEdge)) {
        return Qt::SizeBDiagCursor;
    }
    if ((edges & RightEdge) && (edges & BottomEdge)) {
        return Qt::SizeFDiagCursor;
    }
    if (edges & (LeftEdge | RightEdge)) {
        return Qt::SizeHorCursor;
    }
    if (edges & (TopEdge | BottomEdge)) {
        return Qt::SizeVerCursor;
    }
    return Qt::ArrowCursor;
}

void AeroWindow::updateCursorFor(const QPoint& position)
{
    const int edges = resizeEdgesAt(position);
    if (edges != NoEdge) {
        setCursor(resizeCursor(edges));
    } else {
        unsetCursor();
    }
}

void AeroWindow::updateContentGeometry()
{
    if (m_content != nullptr) {
        m_content->setGeometry(contentRect());
    }
    updateContentMask();
}

void AeroWindow::updateContentMask()
{
    if (m_content == nullptr || m_state == State::Maximized || m_state == State::Fullscreen) {
        if (m_content != nullptr) {
            m_content->clearMask();
        }
        return;
    }

    const QRect content = contentRect();
    if (content.width() <= 0 || content.height() <= 0) {
        return;
    }

    QPainterPath path;
    const qreal radius = 10.0;
    path.addRoundedRect(QRectF(0, 0, content.width(), content.height()), radius, radius);
    m_content->setMask(QRegion(path.toFillPolygon().toPolygon()));
}

void AeroWindow::resizeEvent(QResizeEvent* event)
{
    if (event != nullptr) {
        QWidget::resizeEvent(event);
    }
    updateContentGeometry();
}

void AeroWindow::moveEvent(QMoveEvent* event)
{
    QWidget::moveEvent(event);
}

void AeroWindow::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    const Theme::ThemeTokens& tokens = Theme::ThemeManager::instance().tokens();
    const Theme::ColorTokens& colors = tokens.color;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const int m = margin();
    const QRectF surface(m, m, width() - 2 * m, height() - 2 * m);
    const qreal radius = (m_state == State::Maximized || m_state == State::Fullscreen) ? 0.0 : kWindowRadius;

    if (m > 0) {
        paintShadow(painter, surface);
    }

    QPainterPath shape;
    shape.addRoundedRect(surface, radius, radius);

    painter.save();
    painter.setClipPath(shape);

    QLinearGradient background(surface.topLeft(), surface.bottomLeft());
    background.setColorAt(0.0, Core::Utils::mix(colors.surfaceRaised, QColor(255, 255, 255), 0.04));
    background.setColorAt(1.0, colors.surfaceRaised);
    painter.fillRect(surface, background);

    if (titleBarHeight() > 0) {
        QLinearGradient titleGradient(surface.topLeft(), QPointF(surface.left(), surface.top() + titleBarHeight()));
        titleGradient.setColorAt(0.0, Core::Utils::withAlpha(colors.glassHighlight, 0.16));
        titleGradient.setColorAt(1.0, QColor(255, 255, 255, 0));
        painter.fillRect(QRectF(surface.left(), surface.top(), surface.width(), titleBarHeight()), titleGradient);

        painter.setPen(QPen(Core::Utils::withAlpha(colors.separator, 1.0), 1.0));
        painter.drawLine(QPointF(surface.left() + 1, surface.top() + titleBarHeight()),
                         QPointF(surface.right() - 1, surface.top() + titleBarHeight()));
    }
    painter.restore();

    QColor border = m_active ? Core::Utils::withAlpha(colors.accent, 0.55)
                             : Core::Utils::withAlpha(colors.glassBorder, 0.9);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(border, m_active ? 1.2 : 1.0));
    QPainterPath borderShape;
    borderShape.addRoundedRect(surface.adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);
    painter.drawPath(borderShape);

    if (titleBarHeight() > 0) {
        paintTrafficLights(painter, surface);
        paintTitle(painter, surface);
    }
}

void AeroWindow::paintShadow(QPainter& painter, const QRectF& surface)
{
    const Theme::ColorTokens& colors = Theme::ThemeManager::instance().tokens().color;
    const qreal baseAlpha = m_active ? 0.34 : 0.20;
    const int layers = 11;

    painter.setPen(Qt::NoPen);
    for (int i = layers; i >= 1; --i) {
        const qreal t = static_cast<qreal>(i) / layers;
        const qreal spread = 1.0 + (1.0 - t) * 15.0;
        QColor color = colors.shadow;
        color.setAlphaF(color.alphaF() * baseAlpha * (1.0 - t * 0.55));
        painter.setBrush(color);
        painter.drawRoundedRect(surface.adjusted(-spread, -spread * 0.5, spread, spread * 1.15),
                                kWindowRadius + spread,
                                kWindowRadius + spread);
    }
}

void AeroWindow::paintTrafficLights(QPainter& painter, const QRectF& surface)
{
    Q_UNUSED(surface);
    const Theme::ColorTokens& colors = Theme::ThemeManager::instance().tokens().color;

    struct ButtonStyle {
        HoverButton button;
        QColor color;
    };
    const ButtonStyle styles[] = {
        {HoverButton::Close, QColor(0xFF, 0x5F, 0x57)},
        {HoverButton::Minimize, QColor(0xFE, 0xBC, 0x2E)},
        {HoverButton::Maximize, QColor(0x28, 0xC8, 0x40)},
    };

    const bool showGlyphs = m_titleBarHovered || m_active;
    painter.save();
    for (const ButtonStyle& style : styles) {
        const QRect rect = trafficLightRect(style.button);
        QColor fill = m_active ? style.color : Core::Utils::mix(style.color, colors.textMuted, 0.62);
        painter.setPen(QPen(Core::Utils::withAlpha(QColor(0, 0, 0), 0.12), 0.8));
        painter.setBrush(fill);
        painter.drawEllipse(QRectF(rect).adjusted(0.5, 0.5, -0.5, -0.5));

        if (showGlyphs) {
            const QColor glyph(45, 35, 45, 200);
            const QRectF glyphRect(rect.center().x() - 4.0, rect.center().y() - 4.0, 8.0, 8.0);
            if (style.button == HoverButton::Close) {
                Theme::IconFactory::paint(painter, QStringLiteral("close"), glyphRect, glyph, 2.6);
            } else if (style.button == HoverButton::Minimize) {
                Theme::IconFactory::paint(painter, QStringLiteral("minus"), glyphRect, glyph, 2.6);
            } else {
                Theme::IconFactory::paint(painter,
                                          m_state == State::Maximized ? QStringLiteral("copy") : QStringLiteral("square"),
                                          glyphRect, glyph, 2.4);
            }
        }
    }
    painter.restore();
}

void AeroWindow::paintTitle(QPainter& painter, const QRectF& surface)
{
    const Theme::ThemeTokens& tokens = Theme::ThemeManager::instance().tokens();
    const Theme::ColorTokens& colors = tokens.color;
    const int barHeight = titleBarHeight();

    QFont titleFont = Theme::ThemeManager::instance().font(13.5, QFont::DemiBold);
    QFont subtitleFont = Theme::ThemeManager::instance().font(10.5, QFont::Medium);

    const int leftPad = static_cast<int>(surface.left()) + 82;
    const int rightPad = static_cast<int>(surface.right()) - 82;
    const int available = qMax(80, rightPad - leftPad);
    const QRect titleArea(leftPad, static_cast<int>(surface.top()) + (m_options.subtitle.isEmpty() ? barHeight / 2 - 10 : 7),
                          available, 18);
    const QRect subtitleArea(leftPad, static_cast<int>(surface.top()) + 24, available, 14);

    const QColor titleColor = m_active ? colors.textPrimary : colors.textSecondary;
    painter.setFont(titleFont);
    painter.setPen(titleColor);
    const QString title = QFontMetrics(titleFont).elidedText(
        m_options.title.isEmpty() ? QStringLiteral("Untitled") : m_options.title,
        Qt::ElideMiddle, available);
    painter.drawText(titleArea, Qt::AlignHCenter | Qt::AlignVCenter, title);

    if (!m_options.subtitle.isEmpty()) {
        painter.setFont(subtitleFont);
        painter.setPen(Core::Utils::withAlpha(colors.textMuted, m_active ? 0.95 : 0.7));
        const QString subtitle = QFontMetrics(subtitleFont).elidedText(
            m_options.subtitle, Qt::ElideRight, available);
        painter.drawText(subtitleArea, Qt::AlignHCenter | Qt::AlignVCenter, subtitle);
    }
}

void AeroWindow::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::RightButton && titleBarRect().contains(event->position().toPoint())) {
        emit contextMenuRequested(m_id, event->globalPosition().toPoint());
        event->accept();
        return;
    }

    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }

    emit activated(m_id);

    const QPoint position = event->position().toPoint();
    const HoverButton button = buttonAt(position);
    if (button == HoverButton::Close) {
        emit closeRequested(m_id);
        event->accept();
        return;
    }
    if (button == HoverButton::Minimize) {
        emit minimizeRequested(m_id);
        event->accept();
        return;
    }
    if (button == HoverButton::Maximize) {
        emit maximizeRequested(m_id, m_state != State::Maximized);
        event->accept();
        return;
    }

    const int edges = resizeEdgesAt(position);
    if (edges != NoEdge) {
        m_resizing = true;
        m_resizeEdges = edges;
        m_resizeStartGlobal = event->globalPosition().toPoint();
        m_resizeStartSurface = m_surfaceGeometry;
        setCursor(resizeCursor(edges));
        event->accept();
        return;
    }

    if (titleBarRect().contains(position) && m_state != State::Maximized && m_state != State::Fullscreen) {
        m_dragging = true;
        m_dragOffset = event->globalPosition().toPoint() - mapToGlobal(QPoint(margin(), margin()));
        m_dragStartGeometry = m_surfaceGeometry;
        event->accept();
        return;
    }

    QWidget::mousePressEvent(event);
}

void AeroWindow::mouseMoveEvent(QMouseEvent* event)
{
    const QPoint global = event->globalPosition().toPoint();

    if (m_resizing) {
        const QPoint delta = global - m_resizeStartGlobal;
        QRect surface = m_resizeStartSurface;

        if (m_resizeEdges & LeftEdge) {
            const int newLeft = qMin(surface.right() - 280, surface.left() + delta.x());
            surface.setLeft(newLeft);
        }
        if (m_resizeEdges & RightEdge) {
            surface.setRight(qMax(surface.left() + 280, surface.right() + delta.x()));
        }
        if (m_resizeEdges & TopEdge) {
            const int newTop = qMin(surface.bottom() - 180, surface.top() + delta.y());
            surface.setTop(newTop);
        }
        if (m_resizeEdges & BottomEdge) {
            surface.setBottom(qMax(surface.top() + 180, surface.bottom() + delta.y()));
        }

        if (parentWidget() != nullptr) {
            surface.moveLeft(qBound(-surface.width() + 120, surface.left(),
                                    parentWidget()->width() - 120));
            surface.moveTop(qBound(0, surface.top(), parentWidget()->height() - 44));
        }

        setSurfaceGeometry(surface);
        event->accept();
        return;
    }

    if (m_dragging) {
        QPoint newTopLeft = parentWidget() != nullptr
            ? parentWidget()->mapFromGlobal(global) - m_dragOffset
            : global - m_dragOffset;

        if (parentWidget() != nullptr) {
            newTopLeft.setX(qBound(-m_surfaceGeometry.width() + 140,
                                   newTopLeft.x(),
                                   parentWidget()->width() - 140));
            newTopLeft.setY(qBound(0, newTopLeft.y(), parentWidget()->height() - 44));
        }

        QRect surface = m_surfaceGeometry;
        surface.moveTopLeft(newTopLeft);
        setSurfaceGeometry(surface);
        event->accept();
        return;
    }

    const QPoint position = event->position().toPoint();
    const bool overTitleBar = titleBarRect().contains(position);
    const HoverButton button = buttonAt(position);

    if (overTitleBar != m_titleBarHovered || button != m_hoverButton) {
        m_titleBarHovered = overTitleBar;
        m_hoverButton = button;
        update();
    }
    updateCursorFor(position);

    QWidget::mouseMoveEvent(event);
}

void AeroWindow::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        if (m_dragging) {
            m_dragging = false;
            emit dragFinished(m_id);
            event->accept();
            return;
        }
        if (m_resizing) {
            m_resizing = false;
            m_resizeEdges = NoEdge;
            emit geometryCommitted(m_id, m_surfaceGeometry);
            event->accept();
            return;
        }
    }
    QWidget::mouseReleaseEvent(event);
}

void AeroWindow::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton
        && titleBarRect().contains(event->position().toPoint())
        && m_options.maximizable) {
        emit maximizeRequested(m_id, m_state != State::Maximized);
        event->accept();
        return;
    }
    QWidget::mouseDoubleClickEvent(event);
}

void AeroWindow::leaveEvent(QEvent* event)
{
    m_titleBarHovered = false;
    m_hoverButton = HoverButton::None;
    unsetCursor();
    update();
    QWidget::leaveEvent(event);
}

void AeroWindow::enterEvent(QEnterEvent* event)
{
    QWidget::enterEvent(event);
}

} // namespace Aero::Compositor
