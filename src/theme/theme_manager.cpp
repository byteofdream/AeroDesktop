#include "aero/theme/theme.h"

#include "aero/core/utils.h"

#include <QFontDatabase>
#include <QGuiApplication>
#include <QStyleHints>

namespace Aero::Theme {

ThemeManager& ThemeManager::instance()
{
    static ThemeManager manager;
    return manager;
}

ThemeManager::ThemeManager()
{
    m_fontFamily = chooseFamily(
        {QStringLiteral("Inter"), QStringLiteral("SF Pro Display"), QStringLiteral("SF Pro Text"),
         QStringLiteral("Segoe UI Variable Display"), QStringLiteral("Segoe UI"),
         QStringLiteral("Noto Sans"), QStringLiteral("DejaVu Sans")},
        QStringLiteral("Sans Serif"));

    m_monospaceFamily = chooseFamily(
        {QStringLiteral("JetBrains Mono"), QStringLiteral("SF Mono"), QStringLiteral("Cascadia Code"),
         QStringLiteral("Fira Code"), QStringLiteral("Noto Sans Mono"),
         QStringLiteral("DejaVu Sans Mono")},
        QStringLiteral("Monospace"));

    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this, [this](Qt::ColorScheme) {
        if (m_appearance == Appearance::Auto) {
            rebuild();
            emit themeChanged();
        }
    });

    rebuild();
}

ThemeManager::~ThemeManager() = default;

QString ThemeManager::chooseFamily(const QStringList& candidates, const QString& fallback)
{
    const QStringList families = QFontDatabase::families();
    for (const QString& candidate : candidates) {
        if (families.contains(candidate, Qt::CaseInsensitive)) {
            return candidate;
        }
    }
    return fallback;
}

void ThemeManager::setAppearance(Appearance appearance)
{
    if (m_appearance == appearance) {
        return;
    }
    m_appearance = appearance;
    rebuild();
    emit themeChanged();
}

Appearance ThemeManager::appearance() const
{
    return m_appearance;
}

bool ThemeManager::isDark() const
{
    return m_isDark;
}

void ThemeManager::setAccentColor(const QColor& color)
{
    if (!color.isValid() || m_accent == color) {
        return;
    }
    m_accent = color;
    rebuild();
    emit themeChanged();
}

QColor ThemeManager::accentColor() const
{
    return m_accent;
}

void ThemeManager::setReducedMotion(bool reduced)
{
    if (m_reducedMotion == reduced) {
        return;
    }
    m_reducedMotion = reduced;
    emit themeChanged();
}

bool ThemeManager::reducedMotion() const
{
    return m_reducedMotion;
}

void ThemeManager::setFontScale(qreal scale)
{
    const qreal clamped = Core::Utils::clamp(scale, 0.8, 1.5);
    if (qFuzzyCompare(m_fontScale, clamped)) {
        return;
    }
    m_fontScale = clamped;
    emit themeChanged();
}

qreal ThemeManager::fontScale() const
{
    return m_fontScale;
}

const ThemeTokens& ThemeManager::tokens() const
{
    return m_tokens;
}

void ThemeManager::rebuild()
{
    bool dark = false;
    switch (m_appearance) {
    case Appearance::Light:
        dark = false;
        break;
    case Appearance::Dark:
        dark = true;
        break;
    case Appearance::Auto: {
        const Qt::ColorScheme scheme = QGuiApplication::styleHints()->colorScheme();
        dark = scheme == Qt::ColorScheme::Dark;
        break;
    }
    }

    m_isDark = dark;
    m_tokens = ThemeTokens {};
    m_tokens.dark = dark;
    m_tokens.color = dark ? darkColors() : lightColors();
    m_tokens.fontFamily = m_fontFamily;
    m_tokens.monospaceFamily = m_monospaceFamily;
    m_tokens.bodySize = 13.5 * m_fontScale;
    m_tokens.captionSize = 11.5 * m_fontScale;
    m_tokens.titleSize = 16.0 * m_fontScale;
    m_tokens.largeTitleSize = 26.0 * m_fontScale;

    ColorTokens& colors = m_tokens.color;
    colors.accent = m_accent;
    colors.accentHover = Core::Utils::mix(m_accent, QColor(255, 255, 255), dark ? 0.14 : 0.08);
    colors.accentPressed = Core::Utils::mix(m_accent, QColor(0, 0, 0), 0.16);
    colors.accentSoft = Core::Utils::withAlpha(m_accent, dark ? 0.24 : 0.14);
    colors.onAccent = Core::Utils::contrastColor(m_accent);
}

ColorTokens ThemeManager::lightColors()
{
    ColorTokens c;
    c.windowBackground = QColor(0xF5, 0xF2, 0xFB);
    c.surface = QColor(255, 255, 255, 214);
    c.surfaceRaised = QColor(255, 255, 255, 236);
    c.surfaceSunken = QColor(240, 236, 249, 180);
    c.glassTint = QColor(255, 255, 255, 112);
    c.glassTintStrong = QColor(255, 255, 255, 178);
    c.glassBorder = QColor(255, 255, 255, 190);
    c.glassHighlight = QColor(255, 255, 255, 230);
    c.textPrimary = QColor(0x20, 0x1C, 0x2E);
    c.textSecondary = QColor(0x5D, 0x57, 0x70);
    c.textMuted = QColor(0x8C, 0x86, 0x9D);
    c.accent = QColor(0x7C, 0x5C, 0xFF);
    c.accentHover = QColor(0x8F, 0x73, 0xFF);
    c.accentPressed = QColor(0x6A, 0x4A, 0xE8);
    c.accentSoft = QColor(0x7C, 0x5C, 0xFF, 36);
    c.onAccent = QColor(255, 255, 255);
    c.success = QColor(0x1F, 0xA9, 0x6A);
    c.warning = QColor(0xE0, 0x91, 0x1A);
    c.danger = QColor(0xE2, 0x4C, 0x5C);
    c.separator = QColor(0x2A, 0x22, 0x44, 26);
    c.shadow = QColor(0x35, 0x1F, 0x66, 64);
    c.wallpaperStart = QColor(0xEA, 0xE3, 0xFA);
    c.wallpaperMid = QColor(0xD8, 0xCC, 0xF6);
    c.wallpaperEnd = QColor(0xC6, 0xD7, 0xF8);
    c.wallpaperGlow = QColor(0x9C, 0x7B, 0xFF);
    return c;
}

ColorTokens ThemeManager::darkColors()
{
    ColorTokens c;
    c.windowBackground = QColor(0x0D, 0x0B, 0x14);
    c.surface = QColor(32, 28, 46, 220);
    c.surfaceRaised = QColor(40, 35, 56, 238);
    c.surfaceSunken = QColor(18, 15, 27, 190);
    c.glassTint = QColor(30, 26, 44, 118);
    c.glassTintStrong = QColor(38, 33, 54, 196);
    c.glassBorder = QColor(255, 255, 255, 34);
    c.glassHighlight = QColor(255, 255, 255, 58);
    c.textPrimary = QColor(0xF3, 0xF0, 0xFF);
    c.textSecondary = QColor(0xBC, 0xB5, 0xD0);
    c.textMuted = QColor(0x81, 0x7A, 0x99);
    c.accent = QColor(0x8E, 0x72, 0xFF);
    c.accentHover = QColor(0xA1, 0x89, 0xFF);
    c.accentPressed = QColor(0x78, 0x5B, 0xEE);
    c.accentSoft = QColor(0x8E, 0x72, 0xFF, 62);
    c.onAccent = QColor(255, 255, 255);
    c.success = QColor(0x4A, 0xD6, 0x9A);
    c.warning = QColor(0xFF, 0xB8, 0x4D);
    c.danger = QColor(0xFF, 0x6B, 0x7A);
    c.separator = QColor(255, 255, 255, 24);
    c.shadow = QColor(0, 0, 0, 130);
    c.wallpaperStart = QColor(0x0A, 0x08, 0x12);
    c.wallpaperMid = QColor(0x16, 0x10, 0x2C);
    c.wallpaperEnd = QColor(0x0B, 0x14, 0x2B);
    c.wallpaperGlow = QColor(0x7B, 0x5C, 0xFF);
    return c;
}

QFont ThemeManager::font(qreal pixelSize, QFont::Weight weight) const
{
    QFont result(m_fontFamily);
    result.setPixelSize(qMax(1, qRound(pixelSize * m_fontScale)));
    result.setWeight(weight);
    result.setHintingPreference(QFont::PreferFullHinting);
    result.setStyleStrategy(QFont::PreferAntialias);
    return result;
}

QFont ThemeManager::bodyFont(QFont::Weight weight) const
{
    return font(m_tokens.bodySize, weight);
}

QFont ThemeManager::titleFont(QFont::Weight weight) const
{
    return font(m_tokens.titleSize, weight);
}

QFont ThemeManager::captionFont(QFont::Weight weight) const
{
    return font(m_tokens.captionSize, weight);
}

QFont ThemeManager::monospaceFont(qreal pixelSize) const
{
    QFont result(m_monospaceFamily);
    result.setPixelSize(qMax(1, qRound(pixelSize * m_fontScale)));
    result.setStyleHint(QFont::Monospace);
    result.setFixedPitch(true);
    return result;
}

int ThemeManager::duration(int milliseconds) const
{
    if (m_reducedMotion) {
        return 0;
    }
    return milliseconds;
}

Appearance ThemeManager::appearanceFromString(const QString& value, Appearance fallback)
{
    const QString lower = value.toLower();
    if (lower == QStringLiteral("light")) {
        return Appearance::Light;
    }
    if (lower == QStringLiteral("dark")) {
        return Appearance::Dark;
    }
    if (lower == QStringLiteral("auto") || lower == QStringLiteral("system")) {
        return Appearance::Auto;
    }
    return fallback;
}

QString ThemeManager::appearanceToString(Appearance appearance)
{
    switch (appearance) {
    case Appearance::Light: return QStringLiteral("light");
    case Appearance::Dark: return QStringLiteral("dark");
    case Appearance::Auto: return QStringLiteral("auto");
    }
    return QStringLiteral("auto");
}

} // namespace Aero::Theme
