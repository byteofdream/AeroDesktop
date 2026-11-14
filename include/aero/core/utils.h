#pragma once

#include <QColor>
#include <QDateTime>
#include <QSize>
#include <QString>

#include <algorithm>

namespace Aero::Core::Utils {

QString formatBytes(quint64 bytes, int precision = 1);

QString formatDuration(qint64 seconds);

QString formatRelativeTime(const QDateTime& time);

QString elideMiddle(const QString& text, int maxCharacters);

QString initials(const QString& name);

/// Returns black or white, whichever has better contrast with @p background.
QColor contrastColor(const QColor& background);

QColor mix(const QColor& a, const QColor& b, qreal t);

QColor withAlpha(const QColor& color, qreal alpha);

QString colorToCss(const QColor& color);

/// Converts a user supplied path (including "~") to an absolute clean path.
QString absolutePath(const QString& input, const QString& base = {});

QString fileSizeLabel(qint64 bytes);

bool isImageFile(const QString& path);
bool isTextFile(const QString& path);
bool isArchiveFile(const QString& path);

template <typename T>
constexpr T clamp(T value, T low, T high)
{
    return std::max(low, std::min(value, high));
}

/// Smoothstep easing used by custom paint animations.
inline qreal smoothStep(qreal t)
{
    t = clamp(t, 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

inline qreal easeOutCubic(qreal t)
{
    t = clamp(t, 0.0, 1.0);
    const qreal f = 1.0 - t;
    return 1.0 - f * f * f;
}

} // namespace Aero::Core::Utils
