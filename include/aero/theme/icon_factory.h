#pragma once

#include <QColor>
#include <QIcon>
#include <QPixmap>
#include <QRectF>
#include <QString>

class QPainter;

namespace Aero::Theme {

/// Hand-drawn, theme-aware vector icon set.  Aero does not depend on an icon
/// theme or external assets: every glyph is rendered with QPainter and can be
/// tinted for light/dark surfaces.  Application icons add a gradient tile with
/// the same glyph language.
class IconFactory {
public:
    static QIcon icon(const QString& name, int size = 20, const QColor& color = {});
    static QIcon appIcon(const QString& appId, int size = 64);
    static QPixmap pixmap(const QString& name, int size, const QColor& color, qreal devicePixelRatio = 2.0);

    static void paint(QPainter& painter,
                      const QString& name,
                      const QRectF& rect,
                      const QColor& color,
                      qreal strokeWidth = 1.7);

    static QString glyphForApp(const QString& appId);
    static QColor appColor(const QString& appId);

    static QStringList names();
};

} // namespace Aero::Theme
