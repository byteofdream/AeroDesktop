#include "aero/graphics/wallpaper.h"

#include "aero/core/utils.h"
#include "aero/theme/theme.h"

#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>

namespace Aero::Graphics::Wallpaper {

QStringList presetIds()
{
    return {
        QStringLiteral("aurora"),
        QStringLiteral("dusk"),
        QStringLiteral("ocean"),
        QStringLiteral("graphite"),
    };
}

QString displayName(const QString& id)
{
    if (id == QStringLiteral("aurora")) {
        return QStringLiteral("Aurora");
    }
    if (id == QStringLiteral("dusk")) {
        return QStringLiteral("Dusk");
    }
    if (id == QStringLiteral("ocean")) {
        return QStringLiteral("Ocean");
    }
    if (id == QStringLiteral("graphite")) {
        return QStringLiteral("Graphite");
    }
    return QStringLiteral("Custom");
}

QColor accentColor(const QString& id)
{
    if (id == QStringLiteral("dusk")) {
        return QColor(0xFF, 0x73, 0xB0);
    }
    if (id == QStringLiteral("ocean")) {
        return QColor(0x2F, 0xC7, 0xD8);
    }
    if (id == QStringLiteral("graphite")) {
        return QColor(0xA7, 0x9B, 0xC9);
    }
    return QColor(0x8E, 0x72, 0xFF);
}

void paintPreset(QPainter& painter,
                 const QRectF& rect,
                 const QString& id,
                 bool dark,
                 bool withGlow)
{
    if (rect.width() <= 0.0 || rect.height() <= 0.0) {
        return;
    }

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);

    QColor start;
    QColor mid;
    QColor end;
    QColor glow = accentColor(id);

    if (id == QStringLiteral("dusk")) {
        start = dark ? QColor(0x17, 0x0B, 0x1F) : QColor(0xFD, 0xE7, 0xF2);
        mid = dark ? QColor(0x3A, 0x15, 0x33) : QColor(0xF6, 0xC7, 0xDF);
        end = dark ? QColor(0x12, 0x1B, 0x3A) : QColor(0xD8, 0xDB, 0xFA);
        glow = dark ? QColor(0xFF, 0x5F, 0xA8) : QColor(0xFF, 0x9A, 0xC4);
    } else if (id == QStringLiteral("ocean")) {
        start = dark ? QColor(0x03, 0x12, 0x1B) : QColor(0xDF, 0xF7, 0xFA);
        mid = dark ? QColor(0x06, 0x2A, 0x38) : QColor(0xBF, 0xE9, 0xF0);
        end = dark ? QColor(0x07, 0x1F, 0x3A) : QColor(0xC8, 0xDE, 0xFB);
        glow = dark ? QColor(0x25, 0xC8, 0xD8) : QColor(0x63, 0xD9, 0xE5);
    } else if (id == QStringLiteral("graphite")) {
        start = dark ? QColor(0x0B, 0x0B, 0x0F) : QColor(0xF0, 0xEF, 0xF4);
        mid = dark ? QColor(0x1A, 0x18, 0x22) : QColor(0xDF, 0xDD, 0xE7);
        end = dark ? QColor(0x11, 0x15, 0x1E) : QColor(0xD3, 0xD7, 0xE2);
        glow = dark ? QColor(0x77, 0x6B, 0xA8) : QColor(0xB6, 0xA9, 0xE0);
    } else {
        start = dark ? QColor(0x09, 0x07, 0x12) : QColor(0xEC, 0xE5, 0xFB);
        mid = dark ? QColor(0x18, 0x11, 0x30) : QColor(0xDB, 0xCF, 0xF8);
        end = dark ? QColor(0x0A, 0x15, 0x2C) : QColor(0xC7, 0xD9, 0xFA);
        glow = dark ? QColor(0x7B, 0x5C, 0xFF) : QColor(0x9C, 0x7B, 0xFF);
    }

    QLinearGradient gradient(rect.topLeft(), rect.bottomRight());
    gradient.setColorAt(0.0, start);
    gradient.setColorAt(0.52, mid);
    gradient.setColorAt(1.0, end);
    painter.fillRect(rect, gradient);

    if (withGlow) {
        const QPointF center(rect.left() + rect.width() * 0.78,
                             rect.top() + rect.height() * 0.30);
        const qreal radius = qMax(rect.width(), rect.height()) * 0.55;
        QRadialGradient halo(center, radius);
        QColor haloColor = glow;
        haloColor.setAlphaF(dark ? 0.30 : 0.36);
        halo.setColorAt(0.0, haloColor);
        halo.setColorAt(0.55, Core::Utils::withAlpha(glow, dark ? 0.09 : 0.12));
        halo.setColorAt(1.0, QColor(0, 0, 0, 0));
        painter.fillRect(rect, halo);

        const QPointF second(rect.left() + rect.width() * 0.18,
                             rect.top() + rect.height() * 0.86);
        QRadialGradient lower(second, rect.width() * 0.42);
        QColor lowerColor = dark ? QColor(0x3D, 0x2E, 0x8F) : QColor(0xB8, 0xA4, 0xFF);
        lowerColor.setAlphaF(dark ? 0.18 : 0.22);
        lower.setColorAt(0.0, lowerColor);
        lower.setColorAt(1.0, QColor(0, 0, 0, 0));
        painter.fillRect(rect, lower);
    }

    // A very subtle vignette keeps white window chrome readable on light
    // wallpapers without flattening the glow.
    if (dark) {
        QRadialGradient vignette(rect.center(), qMax(rect.width(), rect.height()) * 0.72);
        vignette.setColorAt(0.55, QColor(0, 0, 0, 0));
        vignette.setColorAt(1.0, QColor(0, 0, 0, 78));
        painter.fillRect(rect, vignette);
    } else {
        QLinearGradient sheen(rect.topLeft(), rect.bottomLeft());
        sheen.setColorAt(0.0, QColor(255, 255, 255, 26));
        sheen.setColorAt(0.45, QColor(255, 255, 255, 0));
        painter.fillRect(rect, sheen);
    }

    painter.restore();
}

} // namespace Aero::Graphics::Wallpaper
