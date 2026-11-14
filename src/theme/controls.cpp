#include "aero/theme/controls.h"

#include "aero/core/utils.h"
#include "aero/theme/icon_factory.h"
#include "aero/theme/theme.h"

#include <QEnterEvent>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>
#include <QWheelEvent>

namespace Aero::Theme {

IconButton::IconButton(const QString& iconName, QWidget* parent)
    : QAbstractButton(parent)
    , m_iconName(iconName)
{
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::NoFocus);
    setAttribute(Qt::WA_Hover, true);
}

IconButton::IconButton(QWidget* parent)
    : IconButton(QStringLiteral("apps"), parent)
{
}

void IconButton::setIconName(const QString& iconName)
{
    if (m_iconName == iconName) {
        return;
    }
    m_iconName = iconName;
    update();
}

QString IconButton::iconName() const
{
    return m_iconName;
}

void IconButton::setIconSize(int size)
{
    m_iconSize = qMax(8, size);
    updateGeometry();
    update();
}

int IconButton::iconSizeValue() const
{
    return m_iconSize;
}

void IconButton::setAccentWhenChecked(bool accent)
{
    m_accentWhenChecked = accent;
    update();
}

void IconButton::setRoundBackground(bool round)
{
    m_round = round;
    update();
}

QSize IconButton::sizeHint() const
{
    return QSize(m_iconSize + 16, m_iconSize + 16);
}

void IconButton::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    const ColorTokens& colors = ThemeManager::instance().tokens().color;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const bool active = isChecked() && m_accentWhenChecked;
    QColor background = Qt::transparent;
    if (active) {
        background = Core::Utils::withAlpha(colors.accent, 0.20);
    } else if (m_hovered && isEnabled()) {
        background = Core::Utils::withAlpha(colors.textPrimary, isDown() ? 0.11 : 0.07);
    }

    if (background.alpha() > 0) {
        const QRectF area = QRectF(rect()).adjusted(1.0, 1.0, -1.0, -1.0);
        painter.setPen(Qt::NoPen);
        painter.setBrush(background);
        painter.drawRoundedRect(area, m_round ? area.height() / 2.0 : 9.0, m_round ? area.height() / 2.0 : 9.0);
    }

    QColor tint = active ? colors.accent : colors.textSecondary;
    if (!isEnabled()) {
        tint = colors.textMuted;
        tint.setAlphaF(tint.alphaF() * 0.55);
    } else if (m_hovered) {
        tint = active ? colors.accentHover : colors.textPrimary;
    }

    const qreal side = qMin(width(), height()) - 10.0;
    const QRectF glyphRect((width() - side) / 2.0, (height() - side) / 2.0, side, side);
    IconFactory::paint(painter, m_iconName, glyphRect, tint, 1.65);
}

void IconButton::enterEvent(QEnterEvent* event)
{
    m_hovered = true;
    update();
    QAbstractButton::enterEvent(event);
}

void IconButton::leaveEvent(QEvent* event)
{
    m_hovered = false;
    update();
    QAbstractButton::leaveEvent(event);
}

AeroButton::AeroButton(const QString& text, QWidget* parent)
    : QAbstractButton(parent)
{
    setText(text);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_Hover, true);
}

AeroButton::AeroButton(const QString& text, const QString& iconName, QWidget* parent)
    : AeroButton(text, parent)
{
    m_iconName = iconName;
}

void AeroButton::setVariant(Variant variant)
{
    m_variant = variant;
    update();
}

AeroButton::Variant AeroButton::variant() const
{
    return m_variant;
}

void AeroButton::setIconName(const QString& iconName)
{
    m_iconName = iconName;
    update();
}

void AeroButton::setCheckableLook(bool checkableLook)
{
    m_checkableLook = checkableLook;
    update();
}

QSize AeroButton::sizeHint() const
{
    const QFontMetrics metrics(font());
    const int textWidth = metrics.horizontalAdvance(text());
    const int iconWidth = m_iconName.isEmpty() ? 0 : 18;
    return QSize(textWidth + iconWidth + 30, 32);
}

void AeroButton::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    const ThemeTokens& tokens = ThemeManager::instance().tokens();
    const ColorTokens& colors = tokens.color;
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRectF area = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    const int radius = tokens.radiusSmall;
    const bool checked = m_checkableLook && isChecked();
    const bool hovered = m_hovered && isEnabled();

    QColor fill = colors.surface;
    QColor border = colors.glassBorder;
    QColor textColor = colors.textPrimary;

    switch (m_variant) {
    case Variant::Primary:
        fill = isDown() ? colors.accentPressed : (hovered ? colors.accentHover : colors.accent);
        border = Qt::transparent;
        textColor = colors.onAccent;
        break;
    case Variant::Ghost:
        fill = (checked ? colors.accentSoft : Qt::transparent);
        border = Qt::transparent;
        textColor = checked ? colors.accent : colors.textSecondary;
        if (hovered) {
            fill = checked ? Core::Utils::withAlpha(colors.accent, 0.24)
                           : Core::Utils::withAlpha(colors.textPrimary, 0.06);
            textColor = checked ? colors.accent : colors.textPrimary;
        }
        break;
    case Variant::Danger:
        fill = isDown() ? Core::Utils::mix(colors.danger, QColor(0, 0, 0), 0.16)
                        : (hovered ? Core::Utils::mix(colors.danger, QColor(255, 255, 255), 0.10)
                                   : colors.danger);
        border = Qt::transparent;
        textColor = QColor(255, 255, 255);
        break;
    case Variant::Secondary:
        fill = hovered ? Core::Utils::withAlpha(colors.textPrimary, 0.07) : colors.surface;
        textColor = checked ? colors.accent : colors.textPrimary;
        if (checked) {
            fill = colors.accentSoft;
            border = Core::Utils::withAlpha(colors.accent, 0.45);
        }
        break;
    }

    if (!isEnabled()) {
        fill.setAlphaF(fill.alphaF() * 0.55);
        textColor = colors.textMuted;
        border = colors.separator;
    }

    painter.setPen(border.alpha() > 0 ? QPen(border, 1.0) : QPen(Qt::NoPen));
    painter.setBrush(fill);
    painter.drawRoundedRect(area, radius, radius);

    const int iconSpace = m_iconName.isEmpty() ? 0 : 20;
    const QRectF textRect = area.adjusted(14, 0, -14, 0);
    if (!m_iconName.isEmpty()) {
        const QRectF iconRect(area.left() + 11, area.center().y() - 8, 16, 16);
        IconFactory::paint(painter, m_iconName, iconRect, textColor, 1.6);
    }

    painter.setPen(textColor);
    painter.setFont(font());
    painter.drawText(textRect.adjusted(iconSpace, 0, 0, 0), Qt::AlignCenter, text());
}

void AeroButton::enterEvent(QEnterEvent* event)
{
    m_hovered = true;
    update();
    QAbstractButton::enterEvent(event);
}

void AeroButton::leaveEvent(QEvent* event)
{
    m_hovered = false;
    update();
    QAbstractButton::leaveEvent(event);
}

ToggleSwitch::ToggleSwitch(QWidget* parent)
    : QAbstractButton(parent)
{
    setCheckable(true);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);
    setFixedSize(sizeHint());
    m_animation = new QPropertyAnimation(this, "knobPosition", this);
    m_animation->setDuration(ThemeManager::instance().duration(150));
    m_animation->setEasingCurve(QEasingCurve::OutCubic);
    connect(this, &QAbstractButton::toggled, this, &ToggleSwitch::animateTo);
}

QSize ToggleSwitch::sizeHint() const
{
    return QSize(44, 26);
}

qreal ToggleSwitch::knobPosition() const
{
    return m_knobPosition;
}

void ToggleSwitch::setKnobPosition(qreal position)
{
    m_knobPosition = Core::Utils::clamp(position, 0.0, 1.0);
    update();
}

void ToggleSwitch::animateTo(bool checked)
{
    const int duration = ThemeManager::instance().duration(150);
    if (duration <= 0 || m_animation == nullptr) {
        setKnobPosition(checked ? 1.0 : 0.0);
        return;
    }
    m_animation->stop();
    m_animation->setStartValue(m_knobPosition);
    m_animation->setEndValue(checked ? 1.0 : 0.0);
    m_animation->setDuration(duration);
    m_animation->start();
}

void ToggleSwitch::enterEvent(QEnterEvent* event)
{
    m_hovered = true;
    update();
    QAbstractButton::enterEvent(event);
}

void ToggleSwitch::leaveEvent(QEvent* event)
{
    m_hovered = false;
    update();
    QAbstractButton::leaveEvent(event);
}

void ToggleSwitch::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    const ColorTokens& colors = ThemeManager::instance().tokens().color;
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRectF track(1.0, (height() - 24.0) / 2.0, width() - 2.0, 24.0);

    QColor off = Core::Utils::withAlpha(colors.textPrimary, 0.16);
    if (m_hovered && isEnabled()) {
        off = Core::Utils::withAlpha(colors.textPrimary, 0.22);
    }
    const QColor on = isDown() ? colors.accentPressed : colors.accent;
    QColor trackColor = Core::Utils::mix(off, on, m_knobPosition);
    if (!isEnabled()) {
        trackColor.setAlphaF(trackColor.alphaF() * 0.55);
    }

    painter.setPen(Qt::NoPen);
    painter.setBrush(trackColor);
    painter.drawRoundedRect(track, track.height() / 2.0, track.height() / 2.0);

    const qreal knobDiameter = 20.0;
    const qreal margin = 2.0;
    const qreal travel = track.width() - knobDiameter - margin * 2.0;
    const QPointF center(track.left() + margin + knobDiameter / 2.0 + travel * m_knobPosition,
                         track.center().y());

    QColor knob = QColor(255, 255, 255);
    if (!isEnabled()) {
        knob = QColor(235, 232, 244);
    }
    painter.setBrush(knob);
    painter.setPen(QPen(Core::Utils::withAlpha(QColor(0, 0, 0), 0.12), 1.0));
    painter.drawEllipse(center, knobDiameter / 2.0, knobDiameter / 2.0);

    if (hasFocus()) {
        painter.setPen(QPen(Core::Utils::withAlpha(colors.accent, 0.55), 1.4));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(QRectF(rect()).adjusted(0.7, 0.7, -0.7, -0.7),
                                height() / 2.0, height() / 2.0);
    }
}

AeroSlider::AeroSlider(QWidget* parent)
    : QWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setCursor(Qt::PointingHandCursor);
    setAttribute(Qt::WA_Hover, true);
}

AeroSlider::AeroSlider(int minimum, int maximum, int value, QWidget* parent)
    : AeroSlider(parent)
{
    setRange(minimum, maximum);
    setValue(value);
}

int AeroSlider::minimum() const
{
    return m_minimum;
}

int AeroSlider::maximum() const
{
    return m_maximum;
}

int AeroSlider::value() const
{
    return m_value;
}

void AeroSlider::setRange(int minimum, int maximum)
{
    m_minimum = minimum;
    m_maximum = qMax(minimum, maximum);
    m_value = Core::Utils::clamp(m_value, m_minimum, m_maximum);
    update();
}

void AeroSlider::setValue(int value)
{
    const int clamped = Core::Utils::clamp(value, m_minimum, m_maximum);
    if (clamped == m_value) {
        return;
    }
    m_value = clamped;
    update();
    emit valueChanged(m_value);
}

void AeroSlider::setShowValueLabel(bool show)
{
    m_showValue = show;
    update();
}

bool AeroSlider::showValueLabel() const
{
    return m_showValue;
}

void AeroSlider::setAccentColor(const QColor& color)
{
    m_accent = color;
    update();
}

QSize AeroSlider::sizeHint() const
{
    return QSize(200, 28);
}

QRectF AeroSlider::trackRect() const
{
    const qreal right = m_showValue ? width() - 42.0 : width() - 8.0;
    return QRectF(8.0, height() / 2.0 - 2.0, qMax(10.0, right - 8.0), 4.0);
}

int AeroSlider::valueForX(qreal x) const
{
    const QRectF track = trackRect();
    const qreal t = Core::Utils::clamp((x - track.left()) / qMax(1.0, track.width()), 0.0, 1.0);
    return m_minimum + qRound(t * (m_maximum - m_minimum));
}

void AeroSlider::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    const ColorTokens& colors = ThemeManager::instance().tokens().color;
    const QColor accent = m_accent.isValid() ? m_accent : colors.accent;
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRectF track = trackRect();
    const qreal t = (m_maximum == m_minimum)
        ? 0.0
        : static_cast<qreal>(m_value - m_minimum) / static_cast<qreal>(m_maximum - m_minimum);
    const qreal filledWidth = track.width() * t;

    painter.setPen(Qt::NoPen);
    painter.setBrush(Core::Utils::withAlpha(colors.textPrimary, 0.12));
    painter.drawRoundedRect(track, 2.0, 2.0);

    painter.setBrush(accent);
    painter.drawRoundedRect(QRectF(track.left(), track.top(), filledWidth, track.height()), 2.0, 2.0);

    const QPointF knob(track.left() + filledWidth, track.center().y());
    const qreal radius = m_dragging || m_hovered ? 8.0 : 7.0;
    painter.setBrush(accent);
    painter.drawEllipse(knob, radius, radius);
    painter.setBrush(QColor(255, 255, 255));
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(knob, radius - 3.0, radius - 3.0);

    if (m_showValue) {
        painter.setPen(colors.textSecondary);
        painter.setFont(ThemeManager::instance().captionFont(QFont::DemiBold));
        painter.drawText(QRectF(width() - 38.0, 0, 36.0, height()),
                         Qt::AlignVCenter | Qt::AlignRight,
                         QString::number(m_value));
    }
}

void AeroSlider::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }
    m_dragging = true;
    setValue(valueForX(event->position().x()));
    event->accept();
}

void AeroSlider::mouseMoveEvent(QMouseEvent* event)
{
    if (m_dragging) {
        setValue(valueForX(event->position().x()));
    }
    QWidget::mouseMoveEvent(event);
}

void AeroSlider::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = false;
        setValue(valueForX(event->position().x()));
        update();
    }
    QWidget::mouseReleaseEvent(event);
}

void AeroSlider::wheelEvent(QWheelEvent* event)
{
    const int steps = event->angleDelta().y() / 120;
    if (steps == 0) {
        event->ignore();
        return;
    }
    setValue(m_value + steps * qMax(1, (m_maximum - m_minimum) / 100));
    event->accept();
}

void AeroSlider::keyPressEvent(QKeyEvent* event)
{
    switch (event->key()) {
    case Qt::Key_Left:
    case Qt::Key_Down:
        setValue(m_value - qMax(1, (m_maximum - m_minimum) / 100));
        event->accept();
        break;
    case Qt::Key_Right:
    case Qt::Key_Up:
        setValue(m_value + qMax(1, (m_maximum - m_minimum) / 100));
        event->accept();
        break;
    case Qt::Key_Home:
        setValue(m_minimum);
        event->accept();
        break;
    case Qt::Key_End:
        setValue(m_maximum);
        event->accept();
        break;
    default:
        QWidget::keyPressEvent(event);
        break;
    }
}

void AeroSlider::enterEvent(QEnterEvent* event)
{
    m_hovered = true;
    update();
    QWidget::enterEvent(event);
}

void AeroSlider::leaveEvent(QEvent* event)
{
    m_hovered = false;
    update();
    QWidget::leaveEvent(event);
}

Card::Card(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
}

void Card::setRadius(int radius)
{
    m_radius = qMax(0, radius);
    update();
}

int Card::radius() const
{
    return m_radius;
}

void Card::setPadding(int horizontal, int vertical)
{
    m_horizontalPadding = horizontal;
    m_verticalPadding = vertical;
}

int Card::horizontalPadding() const
{
    return m_horizontalPadding;
}

int Card::verticalPadding() const
{
    return m_verticalPadding;
}

void Card::setSurfaceColor(const QColor& color)
{
    m_surface = color;
    update();
}

void Card::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    const ColorTokens& colors = ThemeManager::instance().tokens().color;
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRectF area = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    QColor surface = m_surface.isValid() ? m_surface : colors.surface;
    painter.setPen(QPen(Core::Utils::withAlpha(colors.glassBorder, 0.85), 1.0));
    painter.setBrush(surface);
    painter.drawRoundedRect(area, m_radius, m_radius);
}

SegmentedControl::SegmentedControl(const QStringList& options, QWidget* parent)
    : QWidget(parent)
    , m_options(options)
{
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::PointingHandCursor);
    setAttribute(Qt::WA_Hover, true);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
}

void SegmentedControl::setOptions(const QStringList& options)
{
    m_options = options;
    m_current = Core::Utils::clamp(m_current, 0, qMax(0, int(m_options.size()) - 1));
    updateGeometry();
    update();
}

QStringList SegmentedControl::options() const
{
    return m_options;
}

int SegmentedControl::currentIndex() const
{
    return m_current;
}

void SegmentedControl::setCurrentIndex(int index)
{
    if (m_options.isEmpty()) {
        return;
    }
    const int clamped = Core::Utils::clamp(index, 0, int(m_options.size()) - 1);
    if (clamped == m_current) {
        return;
    }
    m_current = clamped;
    update();
    emit currentIndexChanged(m_current);
}

QSize SegmentedControl::sizeHint() const
{
    const QFontMetrics metrics(ThemeManager::instance().bodyFont());
    int width = 16;
    for (const QString& option : m_options) {
        width += metrics.horizontalAdvance(option) + 24;
    }
    return QSize(width, 32);
}

void SegmentedControl::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    const ThemeTokens& tokens = ThemeManager::instance().tokens();
    const ColorTokens& colors = tokens.color;
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRectF area = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    painter.setPen(QPen(colors.separator, 1.0));
    painter.setBrush(Core::Utils::withAlpha(colors.textPrimary, 0.055));
    painter.drawRoundedRect(area, 9.0, 9.0);

    if (m_options.isEmpty()) {
        return;
    }

    const qreal segmentWidth = area.width() / m_options.size();
    if (m_current >= 0 && m_current < m_options.size()) {
        QRectF selected(area.left() + segmentWidth * m_current + 2.0,
                        area.top() + 2.0,
                        segmentWidth - 4.0,
                        area.height() - 4.0);
        painter.setPen(QPen(Core::Utils::withAlpha(colors.glassBorder, 0.9), 1.0));
        painter.setBrush(colors.surfaceRaised);
        painter.drawRoundedRect(selected, 7.0, 7.0);
    }

    painter.setFont(ThemeManager::instance().bodyFont(QFont::DemiBold));
    for (int i = 0; i < m_options.size(); ++i) {
        const QRectF segment(area.left() + segmentWidth * i, area.top(), segmentWidth, area.height());
        painter.setPen(i == m_current ? colors.textPrimary
                                     : (i == m_hovered ? colors.textSecondary : colors.textMuted));
        painter.drawText(segment, Qt::AlignCenter, m_options.at(i));
    }
}

void SegmentedControl::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        const int index = indexAt(event->position());
        if (index >= 0) {
            setCurrentIndex(index);
        }
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void SegmentedControl::mouseMoveEvent(QMouseEvent* event)
{
    const int index = indexAt(event->position());
    if (index != m_hovered) {
        m_hovered = index;
        update();
    }
    QWidget::mouseMoveEvent(event);
}

void SegmentedControl::leaveEvent(QEvent* event)
{
    m_hovered = -1;
    update();
    QWidget::leaveEvent(event);
}

void SegmentedControl::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Left) {
        setCurrentIndex(m_current - 1);
        event->accept();
    } else if (event->key() == Qt::Key_Right) {
        setCurrentIndex(m_current + 1);
        event->accept();
    } else {
        QWidget::keyPressEvent(event);
    }
}

int SegmentedControl::indexAt(const QPointF& position) const
{
    if (m_options.isEmpty() || width() <= 0) {
        return -1;
    }
    const int index = static_cast<int>(position.x() / (width() / m_options.size()));
    return (index >= 0 && index < m_options.size()) ? index : -1;
}

StatusPill::StatusPill(const QString& text, QWidget* parent)
    : QWidget(parent)
    , m_text(text)
    , m_tone(ThemeManager::instance().tokens().color.success)
{
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

void StatusPill::setText(const QString& text)
{
    m_text = text;
    updateGeometry();
    update();
}

void StatusPill::setTone(const QColor& color)
{
    m_tone = color;
    update();
}

void StatusPill::setFilled(bool filled)
{
    m_filled = filled;
    update();
}

QSize StatusPill::sizeHint() const
{
    const QFontMetrics metrics(ThemeManager::instance().captionFont(QFont::DemiBold));
    return QSize(metrics.horizontalAdvance(m_text) + 20, 22);
}

void StatusPill::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    const QRectF area = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    const QColor tone = m_tone.isValid() ? m_tone : ThemeManager::instance().tokens().color.success;
    painter.setPen(QPen(Core::Utils::withAlpha(tone, 0.45), 1.0));
    painter.setBrush(m_filled ? tone : Core::Utils::withAlpha(tone, 0.14));
    painter.drawRoundedRect(area, area.height() / 2.0, area.height() / 2.0);
    painter.setPen(m_filled ? Core::Utils::contrastColor(tone) : tone);
    painter.setFont(ThemeManager::instance().captionFont(QFont::DemiBold));
    painter.drawText(area, Qt::AlignCenter, m_text);
}

} // namespace Aero::Theme
