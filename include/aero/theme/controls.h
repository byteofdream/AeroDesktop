#pragma once

#include <QAbstractButton>
#include <QColor>
#include <QStringList>
#include <QWidget>

class QPropertyAnimation;

namespace Aero::Theme {

/// Square, borderless icon button used by the top bar, window chrome and
/// application toolbars.  Painting is delegated to IconFactory so the button
/// follows the active theme automatically.
class IconButton : public QAbstractButton {
    Q_OBJECT
public:
    explicit IconButton(const QString& iconName, QWidget* parent = nullptr);
    explicit IconButton(QWidget* parent = nullptr);

    void setIconName(const QString& iconName);
    QString iconName() const;

    void setIconSize(int size);
    int iconSizeValue() const;

    void setAccentWhenChecked(bool accent);
    void setRoundBackground(bool round);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    QString m_iconName = QStringLiteral("apps");
    int m_iconSize = 16;
    bool m_accentWhenChecked = false;
    bool m_round = true;
    bool m_hovered = false;
};

/// Text button with an optional leading icon and a filled accent variant.
class AeroButton : public QAbstractButton {
    Q_OBJECT
public:
    enum class Variant {
        Secondary,
        Primary,
        Ghost,
        Danger,
    };

    explicit AeroButton(const QString& text, QWidget* parent = nullptr);
    explicit AeroButton(const QString& text, const QString& iconName, QWidget* parent = nullptr);

    void setVariant(Variant variant);
    Variant variant() const;

    void setIconName(const QString& iconName);
    void setCheckableLook(bool checkableLook);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    QString m_iconName;
    Variant m_variant = Variant::Secondary;
    bool m_hovered = false;
    bool m_checkableLook = false;
};

/// Animated iOS/macOS style switch.  The widget is checkable; users can click
/// or press Space to toggle it.
class ToggleSwitch : public QAbstractButton {
    Q_OBJECT
    Q_PROPERTY(qreal knobPosition READ knobPosition WRITE setKnobPosition)
public:
    explicit ToggleSwitch(QWidget* parent = nullptr);

    QSize sizeHint() const override;

    qreal knobPosition() const;
    void setKnobPosition(qreal position);

protected:
    void paintEvent(QPaintEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    void animateTo(bool checked);
    qreal m_knobPosition = 0.0;
    bool m_hovered = false;
    QPropertyAnimation* m_animation = nullptr;
};

/// Flat, accessible slider with an accent filled track.  It deliberately does
/// not use QSlider so the thumb and track match Aero's design language.
class AeroSlider : public QWidget {
    Q_OBJECT
public:
    explicit AeroSlider(QWidget* parent = nullptr);
    AeroSlider(int minimum, int maximum, int value, QWidget* parent = nullptr);

    int minimum() const;
    int maximum() const;
    int value() const;

    void setRange(int minimum, int maximum);
    void setValue(int value);

    void setShowValueLabel(bool show);
    bool showValueLabel() const;

    void setAccentColor(const QColor& color);

    QSize sizeHint() const override;

signals:
    void valueChanged(int value);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    int valueForX(qreal x) const;
    QRectF trackRect() const;

    int m_minimum = 0;
    int m_maximum = 100;
    int m_value = 0;
    bool m_dragging = false;
    bool m_hovered = false;
    bool m_showValue = false;
    QColor m_accent;
};

/// Rounded panel used inside application pages.
class Card : public QWidget {
    Q_OBJECT
public:
    explicit Card(QWidget* parent = nullptr);

    void setRadius(int radius);
    int radius() const;

    void setPadding(int horizontal, int vertical);
    int horizontalPadding() const;
    int verticalPadding() const;

    void setSurfaceColor(const QColor& color);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    int m_radius = 14;
    int m_horizontalPadding = 16;
    int m_verticalPadding = 14;
    QColor m_surface;
};

/// Compact segmented picker for two to five mutually exclusive options.
class SegmentedControl : public QWidget {
    Q_OBJECT
public:
    explicit SegmentedControl(const QStringList& options, QWidget* parent = nullptr);

    void setOptions(const QStringList& options);
    QStringList options() const;

    int currentIndex() const;
    void setCurrentIndex(int index);

    QSize sizeHint() const override;

signals:
    void currentIndexChanged(int index);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    int indexAt(const QPointF& position) const;

    QStringList m_options;
    int m_current = 0;
    int m_hovered = -1;
};

/// Small translucent status pill (for example "Connected" or "Unavailable").
class StatusPill : public QWidget {
    Q_OBJECT
public:
    explicit StatusPill(const QString& text = {}, QWidget* parent = nullptr);

    void setText(const QString& text);
    void setTone(const QColor& color);
    void setFilled(bool filled);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QString m_text;
    QColor m_tone;
    bool m_filled = false;
};

} // namespace Aero::Theme
