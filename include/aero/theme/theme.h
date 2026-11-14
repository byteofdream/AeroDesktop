#pragma once

#include <QColor>
#include <QFont>
#include <QObject>
#include <QString>

namespace Aero::Theme {

enum class Appearance {
    Light,
    Dark,
    Auto,
};

struct ColorTokens {
    QColor windowBackground;
    QColor surface;
    QColor surfaceRaised;
    QColor surfaceSunken;
    QColor glassTint;
    QColor glassTintStrong;
    QColor glassBorder;
    QColor glassHighlight;
    QColor textPrimary;
    QColor textSecondary;
    QColor textMuted;
    QColor accent;
    QColor accentHover;
    QColor accentPressed;
    QColor accentSoft;
    QColor onAccent;
    QColor success;
    QColor warning;
    QColor danger;
    QColor separator;
    QColor shadow;
    QColor wallpaperStart;
    QColor wallpaperMid;
    QColor wallpaperEnd;
    QColor wallpaperGlow;
};

struct ThemeTokens {
    bool dark = false;
    ColorTokens color;
    int radiusSmall = 8;
    int radiusMedium = 12;
    int radiusLarge = 18;
    int radiusXLarge = 26;
    int radiusWindow = 16;
    int spacingTiny = 4;
    int spacingSmall = 8;
    int spacingMedium = 12;
    int spacingLarge = 16;
    int spacingXLarge = 24;
    int spacingHuge = 32;
    int controlHeight = 32;
    int titleBarHeight = 42;
    int dockHeight = 74;
    int topBarHeight = 34;
    int animationFast = 130;
    int animationNormal = 220;
    int animationSlow = 320;
    qreal titleSize = 16.0;
    qreal bodySize = 13.5;
    qreal captionSize = 11.5;
    qreal largeTitleSize = 26.0;
    QString fontFamily;
    QString monospaceFamily;
};

/// Process-wide design token provider.  The visual language is defined once
/// here and shared by the shell, compositor and every application.
class ThemeManager : public QObject {
    Q_OBJECT
public:
    static ThemeManager& instance();

    void setAppearance(Appearance appearance);
    Appearance appearance() const;
    bool isDark() const;

    void setAccentColor(const QColor& color);
    QColor accentColor() const;

    void setReducedMotion(bool reduced);
    bool reducedMotion() const;

    void setFontScale(qreal scale);
    qreal fontScale() const;

    const ThemeTokens& tokens() const;

    QFont font(qreal pixelSize, QFont::Weight weight = QFont::Normal) const;
    QFont bodyFont(QFont::Weight weight = QFont::Normal) const;
    QFont titleFont(QFont::Weight weight = QFont::DemiBold) const;
    QFont captionFont(QFont::Weight weight = QFont::Normal) const;
    QFont monospaceFont(qreal pixelSize) const;

    /// Animation duration after applying the reduced-motion accessibility
    /// preference.  A value of 0 tells callers to update immediately.
    int duration(int milliseconds) const;

    static Appearance appearanceFromString(const QString& value, Appearance fallback = Appearance::Auto);
    static QString appearanceToString(Appearance appearance);

signals:
    void themeChanged();

private:
    ThemeManager();
    ~ThemeManager() override;

    void rebuild();
    static QString chooseFamily(const QStringList& candidates, const QString& fallback);
    static ColorTokens lightColors();
    static ColorTokens darkColors();

    Appearance m_appearance = Appearance::Auto;
    QColor m_accent {0x7C, 0x5C, 0xFF};
    bool m_reducedMotion = false;
    qreal m_fontScale = 1.0;
    ThemeTokens m_tokens;
    bool m_isDark = false;
    QString m_fontFamily;
    QString m_monospaceFamily;
};

} // namespace Aero::Theme
