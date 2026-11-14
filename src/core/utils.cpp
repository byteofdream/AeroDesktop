#include "aero/core/utils.h"
#include "aero/core/paths.h"

#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <QMimeDatabase>
#include <QLocale>

#include <cmath>

namespace Aero::Core::Utils {

QString formatBytes(quint64 bytes, int precision)
{
    static const char* units[] = {"B", "KB", "MB", "GB", "TB", "PB"};
    double value = static_cast<double>(bytes);
    int unit = 0;
    while (value >= 1024.0 && unit < 5) {
        value /= 1024.0;
        ++unit;
    }
    if (unit == 0) {
        return QStringLiteral("%1 %2").arg(static_cast<quint64>(value)).arg(QString::fromLatin1(units[unit]));
    }
    return QStringLiteral("%1 %2")
        .arg(value, 0, 'f', precision)
        .arg(QString::fromLatin1(units[unit]));
}

QString formatDuration(qint64 seconds)
{
    if (seconds < 60) {
        return QStringLiteral("%1 s").arg(seconds);
    }
    const qint64 minutes = seconds / 60;
    if (minutes < 60) {
        return QStringLiteral("%1 min").arg(minutes);
    }
    const qint64 hours = minutes / 60;
    if (hours < 24) {
        return QStringLiteral("%1 h %2 min").arg(hours).arg(minutes % 60);
    }
    return QStringLiteral("%1 d %2 h").arg(hours / 24).arg(hours % 24);
}

QString formatRelativeTime(const QDateTime& time)
{
    if (!time.isValid()) {
        return {};
    }

    const QDateTime now = QDateTime::currentDateTime();
    const qint64 seconds = time.secsTo(now);

    if (seconds < 60) {
        return QStringLiteral("now");
    }
    if (seconds < 3600) {
        const qint64 minutes = seconds / 60;
        return QStringLiteral("%1 min ago").arg(minutes);
    }
    if (seconds < 86400) {
        const qint64 hours = seconds / 3600;
        return QStringLiteral("%1 h ago").arg(hours);
    }
    if (seconds < 604800) {
        return QStringLiteral("%1 d ago").arg(seconds / 86400);
    }
    return QLocale().toString(time.date(), QLocale::ShortFormat);
}

QString elideMiddle(const QString& text, int maxCharacters)
{
    if (maxCharacters <= 3 || text.size() <= maxCharacters) {
        return text;
    }
    const int left = (maxCharacters - 1) / 2;
    const int right = maxCharacters - left - 1;
    return text.left(left) + QStringLiteral("…") + text.right(right);
}

QString initials(const QString& name)
{
    const QStringList words = name.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (words.isEmpty()) {
        return QStringLiteral("?");
    }
    if (words.size() == 1) {
        return words.first().left(1).toUpper();
    }
    return (words.first().left(1) + words.last().left(1)).toUpper();
}

QColor contrastColor(const QColor& background)
{
    // Rec. 709 luminance; works well for both glass tints and accent colors.
    const double luminance = 0.2126 * background.redF()
        + 0.7152 * background.greenF()
        + 0.0722 * background.blueF();
    return luminance > 0.55 ? QColor(0, 0, 0, 220) : QColor(255, 255, 255, 235);
}

QColor mix(const QColor& a, const QColor& b, qreal t)
{
    const qreal clamped = clamp(t, 0.0, 1.0);
    return QColor::fromRgbF(
        a.redF() + (b.redF() - a.redF()) * clamped,
        a.greenF() + (b.greenF() - a.greenF()) * clamped,
        a.blueF() + (b.blueF() - a.blueF()) * clamped,
        a.alphaF() + (b.alphaF() - a.alphaF()) * clamped);
}

QColor withAlpha(const QColor& color, qreal alpha)
{
    QColor copy = color;
    copy.setAlphaF(clamp(alpha, 0.0, 1.0));
    return copy;
}

QString colorToCss(const QColor& color)
{
    return color.name(QColor::HexArgb);
}

QString absolutePath(const QString& input, const QString& base)
{
    QString path = Paths::expand(input);
    if (QDir::isRelativePath(path)) {
        path = QDir(base.isEmpty() ? QDir::homePath() : base).absoluteFilePath(path);
    }
    return QDir::cleanPath(path);
}

QString fileSizeLabel(qint64 bytes)
{
    return formatBytes(static_cast<quint64>(qMax<qint64>(0, bytes)));
}

bool isImageFile(const QString& path)
{
    static const QMimeDatabase database;
    const QMimeType type = database.mimeTypeForFile(path, QMimeDatabase::MatchExtension);
    return type.name().startsWith(QStringLiteral("image/"));
}

bool isTextFile(const QString& path)
{
    static const QMimeDatabase database;
    const QMimeType type = database.mimeTypeForFile(path, QMimeDatabase::MatchExtension);
    return type.name().startsWith(QStringLiteral("text/"))
        || type.name() == QStringLiteral("application/json")
        || type.name() == QStringLiteral("application/xml")
        || type.name() == QStringLiteral("application/javascript");
}

bool isArchiveFile(const QString& path)
{
    const QString name = QFileInfo(path).fileName().toLower();
    static const QStringList suffixes = {
        QStringLiteral(".zip"), QStringLiteral(".tar"), QStringLiteral(".tar.gz"),
        QStringLiteral(".tgz"), QStringLiteral(".tar.bz2"), QStringLiteral(".tbz2"),
        QStringLiteral(".tar.xz"), QStringLiteral(".txz"), QStringLiteral(".7z"),
        QStringLiteral(".rar"), QStringLiteral(".gz"), QStringLiteral(".bz2"),
        QStringLiteral(".xz"), QStringLiteral(".zst"),
    };
    for (const QString& suffix : suffixes) {
        if (name.endsWith(suffix)) {
            return true;
        }
    }
    return false;
}

} // namespace Aero::Core::Utils
