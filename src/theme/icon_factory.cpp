#include "aero/theme/icon_factory.h"

#include "aero/core/utils.h"
#include "aero/theme/theme.h"

#include <QHash>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPolygonF>

#include <cmath>

namespace Aero::Theme {
namespace {

QIcon buildIcon(const QString& name, int size, const QColor& color, qreal stroke)
{
    constexpr qreal ratios[] = {1.0, 2.0};
    QIcon icon;
    for (qreal ratio : ratios) {
        const int pixels = qMax(1, qRound(size * ratio));
        QPixmap pixmap(pixels, pixels);
        pixmap.setDevicePixelRatio(ratio);
        pixmap.fill(Qt::transparent);

        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::TextAntialiasing, true);
        IconFactory::paint(painter, name, QRectF(0, 0, size, size), color, stroke);
        painter.end();

        icon.addPixmap(pixmap);
    }
    return icon;
}

void drawDots(QPainter& painter, std::initializer_list<QPointF> points, qreal radius)
{
    painter.save();
    painter.setBrush(painter.pen().color());
    for (const QPointF& point : points) {
        painter.drawEllipse(point, radius, radius);
    }
    painter.restore();
}

void drawStar(QPainter& painter)
{
    QPolygonF star;
    for (int i = 0; i < 10; ++i) {
        const qreal angle = -M_PI / 2.0 + i * M_PI / 5.0;
        const qreal radius = (i % 2 == 0) ? 9.0 : 3.8;
        star << QPointF(12.0 + std::cos(angle) * radius,
                        12.0 + std::sin(angle) * radius);
    }
    painter.drawPolygon(star);
}

void drawBattery(QPainter& painter, bool charging, int fillPercent, const QColor& color)
{
    Q_UNUSED(color);
    painter.drawRoundedRect(QRectF(2.5, 8, 17, 9), 2.5, 2.5);
    painter.drawLine(QPointF(21.5, 10.5), QPointF(21.5, 14.5));

    const qreal fillWidth = qMax(0.0, 14.0 * fillPercent / 100.0);
    if (fillWidth > 0.0) {
        painter.save();
        QColor fill = painter.pen().color();
        fill.setAlpha(150);
        painter.setBrush(fill);
        painter.setPen(Qt::NoPen);
        painter.drawRoundedRect(QRectF(5, 10.2, fillWidth, 4.6), 1.2, 1.2);
        painter.restore();
    }

    if (charging) {
        QPainterPath bolt;
        bolt.moveTo(12.5, 5.5);
        bolt.lineTo(8.5, 13.0);
        bolt.lineTo(11.8, 13.0);
        bolt.lineTo(10.2, 18.5);
        bolt.lineTo(16.0, 10.5);
        bolt.lineTo(12.6, 10.5);
        bolt.closeSubpath();
        painter.save();
        painter.setBrush(painter.pen().color());
        painter.setPen(Qt::NoPen);
        painter.drawPath(bolt);
        painter.restore();
    }
}

void drawSpeaker(QPainter& painter)
{
    QPainterPath speaker;
    speaker.moveTo(3.0, 9.0);
    speaker.lineTo(7.0, 9.0);
    speaker.lineTo(11.5, 4.5);
    speaker.lineTo(11.5, 19.5);
    speaker.lineTo(7.0, 15.0);
    speaker.lineTo(3.0, 15.0);
    speaker.closeSubpath();
    painter.drawPath(speaker);
}

void drawMonitorBody(QPainter& painter)
{
    painter.drawRoundedRect(QRectF(2.5, 4.0, 19.0, 13.0), 2.0, 2.0);
    painter.drawLine(QPointF(12, 17), QPointF(12, 20.5));
    painter.drawLine(QPointF(8.5, 20.5), QPointF(15.5, 20.5));
}

void drawPage(QPainter& painter)
{
    QPainterPath page;
    page.moveTo(6.0, 2.5);
    page.lineTo(14.5, 2.5);
    page.lineTo(19.0, 7.0);
    page.lineTo(19.0, 21.5);
    page.lineTo(6.0, 21.5);
    page.closeSubpath();
    painter.drawPath(page);
    painter.drawLine(QPointF(14.5, 2.5), QPointF(14.5, 7.0));
    painter.drawLine(QPointF(14.5, 7.0), QPointF(19.0, 7.0));
}

void drawGear(QPainter& painter)
{
    painter.drawEllipse(QPointF(12, 12), 6.4, 6.4);
    painter.drawEllipse(QPointF(12, 12), 2.5, 2.5);
    for (int i = 0; i < 8; ++i) {
        const qreal angle = i * M_PI / 4.0;
        painter.drawLine(QPointF(12 + std::cos(angle) * 7.8, 12 + std::sin(angle) * 7.8),
                         QPointF(12 + std::cos(angle) * 10.2, 12 + std::sin(angle) * 10.2));
    }
}

} // namespace

void IconFactory::paint(QPainter& painter,
                        const QString& name,
                        const QRectF& rect,
                        const QColor& color,
                        qreal strokeWidth)
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.translate(rect.topLeft());
    const qreal scale = qMin(rect.width(), rect.height()) / 24.0;
    painter.scale(scale, scale);

    QPen pen(color);
    pen.setWidthF(strokeWidth);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);

    const QString key = name.toLower();

    if (key == QStringLiteral("close") || key == QStringLiteral("x")) {
        painter.drawLine(QPointF(6, 6), QPointF(18, 18));
        painter.drawLine(QPointF(18, 6), QPointF(6, 18));
    } else if (key == QStringLiteral("minus") || key == QStringLiteral("minimize")) {
        painter.drawLine(QPointF(5.5, 12), QPointF(18.5, 12));
    } else if (key == QStringLiteral("square") || key == QStringLiteral("maximize")) {
        painter.drawRoundedRect(QRectF(6, 6, 12, 12), 2, 2);
    } else if (key == QStringLiteral("plus")) {
        painter.drawLine(QPointF(12, 5), QPointF(12, 19));
        painter.drawLine(QPointF(5, 12), QPointF(19, 12));
    } else if (key == QStringLiteral("check")) {
        QPainterPath path;
        path.moveTo(4.5, 12.5);
        path.lineTo(9.5, 17.5);
        path.lineTo(19.5, 6.5);
        painter.drawPath(path);
    } else if (key == QStringLiteral("search")) {
        painter.drawEllipse(QRectF(4.0, 4.0, 12.0, 12.0));
        painter.drawLine(QPointF(15.0, 15.0), QPointF(20.5, 20.5));
    } else if (key == QStringLiteral("wifi")) {
        painter.drawArc(QRectF(2.5, 7.5, 19, 19), 42 * 16, 96 * 16);
        painter.drawArc(QRectF(6.0, 11.0, 12, 12), 42 * 16, 96 * 16);
        drawDots(painter, {QPointF(12, 18)}, 1.35);
    } else if (key == QStringLiteral("wifi-off")) {
        painter.drawArc(QRectF(2.5, 7.5, 19, 19), 42 * 16, 96 * 16);
        painter.drawArc(QRectF(6.0, 11.0, 12, 12), 42 * 16, 96 * 16);
        drawDots(painter, {QPointF(12, 18)}, 1.35);
        painter.drawLine(QPointF(4, 4), QPointF(20, 20));
    } else if (key == QStringLiteral("bluetooth")) {
        painter.drawLine(QPointF(12, 2.5), QPointF(12, 21.5));
        QPainterPath path;
        path.moveTo(7.0, 7.0);
        path.lineTo(17.0, 17.0);
        path.lineTo(12.0, 21.5);
        path.moveTo(7.0, 17.0);
        path.lineTo(17.0, 7.0);
        path.lineTo(12.0, 2.5);
        painter.drawPath(path);
    } else if (key == QStringLiteral("volume") || key == QStringLiteral("audio")) {
        drawSpeaker(painter);
        painter.drawArc(QRectF(9.5, 7.0, 9.0, 10.0), -55 * 16, 110 * 16);
        painter.drawArc(QRectF(7.5, 4.0, 14.0, 16.0), -55 * 16, 110 * 16);
    } else if (key == QStringLiteral("volume-muted")) {
        drawSpeaker(painter);
        painter.drawLine(QPointF(16, 9), QPointF(21, 15));
        painter.drawLine(QPointF(21, 9), QPointF(16, 15));
    } else if (key == QStringLiteral("battery") || key == QStringLiteral("battery-charging")
               || key == QStringLiteral("battery-low")) {
        const int fill = key == QStringLiteral("battery-low") ? 18 : 68;
        drawBattery(painter, key == QStringLiteral("battery-charging"), fill, color);
    } else if (key == QStringLiteral("bell") || key == QStringLiteral("notifications")) {
        QPainterPath bell;
        bell.moveTo(5.0, 17.0);
        bell.lineTo(7.0, 15.0);
        bell.lineTo(7.0, 10.5);
        bell.cubicTo(7.0, 7.0, 9.2, 4.5, 12.0, 4.5);
        bell.cubicTo(14.8, 4.5, 17.0, 7.0, 17.0, 10.5);
        bell.lineTo(17.0, 15.0);
        bell.lineTo(19.0, 17.0);
        bell.closeSubpath();
        painter.drawPath(bell);
        painter.drawArc(QRectF(9.5, 17.0, 5.0, 4.5), 180 * 16, 180 * 16);
    } else if (key == QStringLiteral("bell-off")) {
        QPainterPath bell;
        bell.moveTo(5.0, 17.0);
        bell.lineTo(7.0, 15.0);
        bell.lineTo(7.0, 10.5);
        bell.cubicTo(7.0, 7.0, 9.2, 4.5, 12.0, 4.5);
        bell.cubicTo(14.8, 4.5, 17.0, 7.0, 17.0, 10.5);
        bell.lineTo(17.0, 15.0);
        bell.lineTo(19.0, 17.0);
        bell.closeSubpath();
        painter.drawPath(bell);
        painter.drawLine(QPointF(3, 3), QPointF(21, 21));
    } else if (key == QStringLiteral("moon") || key == QStringLiteral("night")) {
        QPainterPath moon;
        moon.moveTo(15.5, 3.0);
        moon.cubicTo(10.0, 5.0, 7.0, 9.0, 7.0, 13.0);
        moon.cubicTo(7.0, 17.5, 10.0, 20.5, 14.5, 21.5);
        moon.cubicTo(8.0, 22.0, 3.5, 18.0, 3.5, 12.0);
        moon.cubicTo(3.5, 7.0, 9.0, 3.0, 15.5, 3.0);
        moon.closeSubpath();
        painter.drawPath(moon);
    } else if (key == QStringLiteral("sun")) {
        painter.drawEllipse(QPointF(12, 12), 4.3, 4.3);
        for (int i = 0; i < 8; ++i) {
            const qreal angle = i * M_PI / 4.0;
            painter.drawLine(QPointF(12 + std::cos(angle) * 6.6, 12 + std::sin(angle) * 6.6),
                             QPointF(12 + std::cos(angle) * 9.3, 12 + std::sin(angle) * 9.3));
        }
    } else if (key == QStringLiteral("gear") || key == QStringLiteral("settings")) {
        drawGear(painter);
    } else if (key == QStringLiteral("chevron-left")) {
        QPainterPath path;
        path.moveTo(15, 5);
        path.lineTo(8, 12);
        path.lineTo(15, 19);
        painter.drawPath(path);
    } else if (key == QStringLiteral("chevron-right")) {
        QPainterPath path;
        path.moveTo(9, 5);
        path.lineTo(16, 12);
        path.lineTo(9, 19);
        painter.drawPath(path);
    } else if (key == QStringLiteral("chevron-down")) {
        QPainterPath path;
        path.moveTo(5, 9);
        path.lineTo(12, 16);
        path.lineTo(19, 9);
        painter.drawPath(path);
    } else if (key == QStringLiteral("chevron-up")) {
        QPainterPath path;
        path.moveTo(5, 15);
        path.lineTo(12, 8);
        path.lineTo(19, 15);
        painter.drawPath(path);
    } else if (key == QStringLiteral("power")) {
        painter.drawArc(QRectF(4, 3.5, 16, 16), 38 * 16, 284 * 16);
        painter.drawLine(QPointF(12, 2.5), QPointF(12, 11.5));
    } else if (key == QStringLiteral("lock")) {
        painter.drawArc(QRectF(7.5, 3.0, 9.0, 11.0), 0, 180 * 16);
        painter.drawRoundedRect(QRectF(5, 10, 14, 11), 2.5, 2.5);
        drawDots(painter, {QPointF(12, 15.5)}, 1.2);
    } else if (key == QStringLiteral("user")) {
        painter.drawEllipse(QPointF(12, 7.5), 3.8, 3.8);
        QPainterPath shoulders;
        shoulders.moveTo(4.5, 21.0);
        shoulders.cubicTo(4.5, 15.5, 7.8, 13.5, 12.0, 13.5);
        shoulders.cubicTo(16.2, 13.5, 19.5, 15.5, 19.5, 21.0);
        painter.drawPath(shoulders);
    } else if (key == QStringLiteral("display") || key == QStringLiteral("desktop")) {
        drawMonitorBody(painter);
    } else if (key == QStringLiteral("keyboard")) {
        painter.drawRoundedRect(QRectF(2, 7, 20, 11), 2.0, 2.0);
        drawDots(painter, {QPointF(6, 11), QPointF(10, 11), QPointF(14, 11), QPointF(18, 11),
                           QPointF(8, 15), QPointF(12, 15), QPointF(16, 15)}, 0.9);
        painter.drawLine(QPointF(6, 15.5), QPointF(6, 15.5));
    } else if (key == QStringLiteral("mouse")) {
        painter.drawRoundedRect(QRectF(7, 2.5, 10, 19), 5.0, 5.0);
        painter.drawLine(QPointF(12, 2.8), QPointF(12, 9.5));
    } else if (key == QStringLiteral("accessibility")) {
        painter.drawEllipse(QPointF(12, 4.2), 2.4, 2.4);
        painter.drawLine(QPointF(4, 9.5), QPointF(20, 9.5));
        painter.drawLine(QPointF(12, 7), QPointF(12, 15));
        painter.drawLine(QPointF(12, 15), QPointF(7, 21));
        painter.drawLine(QPointF(12, 15), QPointF(17, 21));
    } else if (key == QStringLiteral("palette")) {
        painter.drawEllipse(QPointF(12, 12), 9.0, 9.0);
        drawDots(painter, {QPointF(8, 8), QPointF(15.5, 7.5), QPointF(17, 13.5), QPointF(8.5, 15.5)}, 1.3);
    } else if (key == QStringLiteral("wallpaper") || key == QStringLiteral("image")
               || key == QStringLiteral("pictures")) {
        painter.drawRoundedRect(QRectF(2.5, 4.5, 19, 15), 2.0, 2.0);
        painter.drawEllipse(QPointF(7.5, 9.0), 1.5, 1.5);
        QPainterPath hills;
        hills.moveTo(4.5, 17.5);
        hills.lineTo(9.5, 11.5);
        hills.lineTo(12.5, 15.0);
        hills.lineTo(15.0, 12.0);
        hills.lineTo(20.0, 17.5);
        painter.drawPath(hills);
    } else if (key == QStringLiteral("dock")) {
        painter.drawRoundedRect(QRectF(2.0, 13.5, 20.0, 8.0), 3.0, 3.0);
        drawDots(painter, {QPointF(7, 17.5), QPointF(12, 17.5), QPointF(17, 17.5)}, 1.2);
    } else if (key == QStringLiteral("apps") || key == QStringLiteral("grid")) {
        painter.drawRoundedRect(QRectF(3.5, 3.5, 7.0, 7.0), 1.8, 1.8);
        painter.drawRoundedRect(QRectF(13.5, 3.5, 7.0, 7.0), 1.8, 1.8);
        painter.drawRoundedRect(QRectF(3.5, 13.5, 7.0, 7.0), 1.8, 1.8);
        painter.drawRoundedRect(QRectF(13.5, 13.5, 7.0, 7.0), 1.8, 1.8);
    } else if (key == QStringLiteral("info") || key == QStringLiteral("about")) {
        painter.drawEllipse(QPointF(12, 12), 9.0, 9.0);
        drawDots(painter, {QPointF(12, 7.8)}, 1.2);
        painter.drawLine(QPointF(12, 11.0), QPointF(12, 16.8));
    } else if (key == QStringLiteral("folder") || key == QStringLiteral("finder")) {
        QPainterPath folder;
        folder.moveTo(3.0, 7.0);
        folder.lineTo(9.0, 7.0);
        folder.lineTo(11.0, 9.0);
        folder.lineTo(21.0, 9.0);
        folder.lineTo(21.0, 19.5);
        folder.lineTo(3.0, 19.5);
        folder.closeSubpath();
        painter.drawPath(folder);
    } else if (key == QStringLiteral("file")) {
        drawPage(painter);
    } else if (key == QStringLiteral("file-text") || key == QStringLiteral("editor")) {
        drawPage(painter);
        painter.drawLine(QPointF(9, 12), QPointF(16, 12));
        painter.drawLine(QPointF(9, 15), QPointF(16, 15));
        painter.drawLine(QPointF(9, 18), QPointF(14, 18));
    } else if (key == QStringLiteral("file-pdf")) {
        drawPage(painter);
        painter.drawLine(QPointF(9, 13), QPointF(16, 13));
        painter.drawLine(QPointF(9, 16), QPointF(16, 16));
        painter.drawLine(QPointF(9, 19), QPointF(13, 19));
    } else if (key == QStringLiteral("video")) {
        painter.drawRoundedRect(QRectF(2.5, 5.0, 19.0, 14.0), 2.5, 2.5);
        QPainterPath play;
        play.moveTo(10, 9);
        play.lineTo(16, 12);
        play.lineTo(10, 15);
        play.closeSubpath();
        painter.drawPath(play);
    } else if (key == QStringLiteral("archive")) {
        painter.drawRoundedRect(QRectF(3, 7, 18, 14), 2.0, 2.0);
        painter.drawLine(QPointF(2, 4), QPointF(22, 4));
        painter.drawLine(QPointF(2, 7), QPointF(22, 7));
        painter.drawLine(QPointF(12, 9), QPointF(12, 18));
        drawDots(painter, {QPointF(12, 16)}, 1.0);
    } else if (key == QStringLiteral("home")) {
        QPainterPath roof;
        roof.moveTo(3, 11);
        roof.lineTo(12, 3);
        roof.lineTo(21, 11);
        painter.drawPath(roof);
        QPainterPath house;
        house.moveTo(5.5, 10);
        house.lineTo(5.5, 21);
        house.lineTo(18.5, 21);
        house.lineTo(18.5, 10);
        painter.drawPath(house);
        painter.drawRoundedRect(QRectF(10, 15, 4, 6), 1.0, 1.0);
    } else if (key == QStringLiteral("documents")) {
        painter.drawRoundedRect(QRectF(7, 2.5, 13, 16), 2.0, 2.0);
        painter.drawRoundedRect(QRectF(4, 6, 13, 16), 2.0, 2.0);
    } else if (key == QStringLiteral("downloads") || key == QStringLiteral("download")) {
        painter.drawLine(QPointF(12, 3), QPointF(12, 14));
        QPainterPath arrow;
        arrow.moveTo(7, 9);
        arrow.lineTo(12, 14);
        arrow.lineTo(17, 9);
        painter.drawPath(arrow);
        painter.drawLine(QPointF(4, 18), QPointF(4, 21));
        painter.drawLine(QPointF(4, 21), QPointF(20, 21));
        painter.drawLine(QPointF(20, 21), QPointF(20, 18));
    } else if (key == QStringLiteral("music")) {
        painter.drawLine(QPointF(10, 17), QPointF(10, 5));
        painter.drawLine(QPointF(10, 5), QPointF(18, 3.5));
        painter.drawLine(QPointF(18, 3.5), QPointF(18, 15));
        drawDots(painter, {QPointF(8, 17), QPointF(16, 15)}, 2.2);
    } else if (key == QStringLiteral("trash")) {
        painter.drawLine(QPointF(4, 7), QPointF(20, 7));
        painter.drawLine(QPointF(9.5, 4), QPointF(14.5, 4));
        QPainterPath can;
        can.moveTo(6, 7);
        can.lineTo(7.5, 21);
        can.lineTo(16.5, 21);
        can.lineTo(18, 7);
        painter.drawPath(can);
        painter.drawLine(QPointF(10, 11), QPointF(10, 17));
        painter.drawLine(QPointF(14, 11), QPointF(14, 17));
    } else if (key == QStringLiteral("back") || key == QStringLiteral("arrow-left")) {
        painter.drawLine(QPointF(20, 12), QPointF(5, 12));
        QPainterPath path;
        path.moveTo(11, 6);
        path.lineTo(5, 12);
        path.lineTo(11, 18);
        painter.drawPath(path);
    } else if (key == QStringLiteral("forward") || key == QStringLiteral("arrow-right")) {
        painter.drawLine(QPointF(4, 12), QPointF(19, 12));
        QPainterPath path;
        path.moveTo(13, 6);
        path.lineTo(19, 12);
        path.lineTo(13, 18);
        painter.drawPath(path);
    } else if (key == QStringLiteral("refresh")) {
        painter.drawArc(QRectF(4, 4, 16, 16), 30 * 16, 300 * 16);
        QPainterPath arrow;
        arrow.moveTo(18.5, 4.5);
        arrow.lineTo(19.5, 9.5);
        arrow.lineTo(14.5, 9.0);
        painter.drawPath(arrow);
    } else if (key == QStringLiteral("play")) {
        QPainterPath play;
        play.moveTo(8, 5);
        play.lineTo(19, 12);
        play.lineTo(8, 19);
        play.closeSubpath();
        painter.save();
        painter.setBrush(painter.pen().color());
        painter.drawPath(play);
        painter.restore();
    } else if (key == QStringLiteral("pause")) {
        painter.save();
        painter.setBrush(painter.pen().color());
        painter.setPen(Qt::NoPen);
        painter.drawRoundedRect(QRectF(7, 5, 3.5, 14), 1.2, 1.2);
        painter.drawRoundedRect(QRectF(13.5, 5, 3.5, 14), 1.2, 1.2);
        painter.restore();
    } else if (key == QStringLiteral("calculator")) {
        painter.drawRoundedRect(QRectF(5, 2.5, 14, 19), 2.5, 2.5);
        painter.drawRoundedRect(QRectF(7, 4.5, 10, 4), 1.0, 1.0);
        drawDots(painter, {QPointF(8, 12), QPointF(12, 12), QPointF(16, 12),
                           QPointF(8, 15.5), QPointF(12, 15.5), QPointF(16, 15.5),
                           QPointF(8, 19), QPointF(12, 19), QPointF(16, 19)}, 1.0);
    } else if (key == QStringLiteral("calendar")) {
        painter.drawRoundedRect(QRectF(3, 5, 18, 16.5), 2.5, 2.5);
        painter.drawLine(QPointF(3, 9.5), QPointF(21, 9.5));
        painter.drawLine(QPointF(7.5, 2.5), QPointF(7.5, 6.5));
        painter.drawLine(QPointF(16.5, 2.5), QPointF(16.5, 6.5));
        drawDots(painter, {QPointF(8, 13.5), QPointF(12, 13.5), QPointF(16, 13.5),
                           QPointF(8, 17.5), QPointF(12, 17.5)}, 0.95);
    } else if (key == QStringLiteral("terminal")) {
        painter.drawRoundedRect(QRectF(2, 3.5, 20, 17), 2.5, 2.5);
        QPainterPath prompt;
        prompt.moveTo(6, 8.5);
        prompt.lineTo(9.5, 12);
        prompt.lineTo(6, 15.5);
        painter.drawPath(prompt);
        painter.drawLine(QPointF(12, 15.5), QPointF(18, 15.5));
    } else if (key == QStringLiteral("monitor")) {
        drawMonitorBody(painter);
        QPainterPath chart;
        chart.moveTo(5, 12);
        chart.lineTo(8, 12);
        chart.lineTo(10, 8);
        chart.lineTo(13, 14.5);
        chart.lineTo(15, 10.5);
        chart.lineTo(19, 10.5);
        painter.drawPath(chart);
    } else if (key == QStringLiteral("activity") || key == QStringLiteral("tasks")) {
        QPainterPath pulse;
        pulse.moveTo(2.5, 12);
        pulse.lineTo(6.5, 12);
        pulse.lineTo(8.5, 6);
        pulse.lineTo(12.5, 18.5);
        pulse.lineTo(15.5, 9);
        pulse.lineTo(17.5, 12);
        pulse.lineTo(21.5, 12);
        painter.drawPath(pulse);
    } else if (key == QStringLiteral("edit")) {
        QPainterPath pencil;
        pencil.moveTo(4, 20);
        pencil.lineTo(5.5, 15);
        pencil.lineTo(15.5, 5);
        pencil.lineTo(19, 8.5);
        pencil.lineTo(9, 18.5);
        pencil.closeSubpath();
        painter.drawPath(pencil);
        painter.drawLine(QPointF(15.5, 5), QPointF(19, 8.5));
    } else if (key == QStringLiteral("save")) {
        QPainterPath floppy;
        floppy.moveTo(3, 3);
        floppy.lineTo(18, 3);
        floppy.lineTo(21, 6);
        floppy.lineTo(21, 21);
        floppy.lineTo(3, 21);
        floppy.closeSubpath();
        painter.drawPath(floppy);
        painter.drawRoundedRect(QRectF(7, 3, 10, 6), 0.8, 0.8);
        painter.drawRoundedRect(QRectF(7, 14, 10, 7), 1.0, 1.0);
    } else if (key == QStringLiteral("copy")) {
        painter.drawRoundedRect(QRectF(8, 8, 13, 13), 2.0, 2.0);
        QPainterPath back;
        back.moveTo(16, 5);
        back.lineTo(5, 5);
        back.lineTo(5, 16);
        painter.drawPath(back);
    } else if (key == QStringLiteral("cut")) {
        painter.drawEllipse(QPointF(6.5, 17.5), 2.6, 2.6);
        painter.drawEllipse(QPointF(6.5, 6.5), 2.6, 2.6);
        painter.drawLine(QPointF(8.5, 15.5), QPointF(20, 4));
        painter.drawLine(QPointF(8.5, 8.5), QPointF(20, 20));
    } else if (key == QStringLiteral("paste")) {
        painter.drawRoundedRect(QRectF(5, 4, 14, 18), 2.0, 2.0);
        painter.drawRoundedRect(QRectF(9, 2, 6, 4), 1.2, 1.2);
        painter.drawLine(QPointF(9, 11), QPointF(15, 11));
        painter.drawLine(QPointF(9, 14), QPointF(15, 14));
    } else if (key == QStringLiteral("undo") || key == QStringLiteral("redo")) {
        const bool undo = key == QStringLiteral("undo");
        if (!undo) {
            painter.save();
            painter.translate(24, 0);
            painter.scale(-1, 1);
        }
        QPainterPath path;
        path.moveTo(5, 8);
        path.cubicTo(14, 7, 20, 10, 20, 17);
        painter.drawPath(path);
        QPainterPath arrow;
        arrow.moveTo(8, 4);
        arrow.lineTo(4, 8);
        arrow.lineTo(8.5, 11.5);
        painter.drawPath(arrow);
        if (!undo) {
            painter.restore();
        }
    } else if (key == QStringLiteral("list")) {
        for (int i = 0; i < 3; ++i) {
            const qreal y = 6.0 + i * 6.0;
            drawDots(painter, {QPointF(5, y)}, 1.2);
            painter.drawLine(QPointF(9, y), QPointF(20, y));
        }
    } else if (key == QStringLiteral("ellipsis")) {
        drawDots(painter, {QPointF(5, 12), QPointF(12, 12), QPointF(19, 12)}, 1.6);
    } else if (key == QStringLiteral("arrow-up")) {
        painter.drawLine(QPointF(12, 21), QPointF(12, 4));
        QPainterPath path;
        path.moveTo(5, 11);
        path.lineTo(12, 4);
        path.lineTo(19, 11);
        painter.drawPath(path);
    } else if (key == QStringLiteral("external")) {
        QPainterPath box;
        box.moveTo(13, 4);
        box.lineTo(5, 4);
        box.lineTo(5, 20);
        box.lineTo(20, 20);
        box.lineTo(20, 12);
        painter.drawPath(box);
        painter.drawLine(QPointF(13, 11), QPointF(20, 4));
        painter.drawLine(QPointF(14, 4), QPointF(20, 4));
        painter.drawLine(QPointF(20, 4), QPointF(20, 10));
    } else if (key == QStringLiteral("eject")) {
        QPainterPath triangle;
        triangle.moveTo(12, 5);
        triangle.lineTo(20, 15);
        triangle.lineTo(4, 15);
        triangle.closeSubpath();
        painter.drawPath(triangle);
        painter.drawLine(QPointF(4, 19), QPointF(20, 19));
    } else if (key == QStringLiteral("star")) {
        drawStar(painter);
    } else if (key == QStringLiteral("clock")) {
        painter.drawEllipse(QPointF(12, 12), 9, 9);
        painter.drawLine(QPointF(12, 7), QPointF(12, 12));
        painter.drawLine(QPointF(12, 12), QPointF(16, 14));
    } else if (key == QStringLiteral("globe") || key == QStringLiteral("network")) {
        painter.drawEllipse(QPointF(12, 12), 9, 9);
        painter.drawLine(QPointF(3, 12), QPointF(21, 12));
        painter.drawEllipse(QPointF(12, 12), 4.0, 9.0);
    } else if (key == QStringLiteral("shield")) {
        QPainterPath shield;
        shield.moveTo(12, 2);
        shield.lineTo(20, 5.5);
        shield.lineTo(19, 13);
        shield.cubicTo(18, 18, 15, 20.5, 12, 22);
        shield.cubicTo(9, 20.5, 6, 18, 5, 13);
        shield.lineTo(4, 5.5);
        shield.closeSubpath();
        painter.drawPath(shield);
    } else if (key == QStringLiteral("command")) {
        painter.drawRoundedRect(QRectF(3, 3, 7, 7), 2.5, 2.5);
        painter.drawRoundedRect(QRectF(14, 3, 7, 7), 2.5, 2.5);
        painter.drawRoundedRect(QRectF(3, 14, 7, 7), 2.5, 2.5);
        painter.drawRoundedRect(QRectF(14, 14, 7, 7), 2.5, 2.5);
    } else {
        painter.drawEllipse(QPointF(12, 12), 8.5, 8.5);
        drawDots(painter, {QPointF(12, 12)}, 1.4);
    }

    painter.restore();
}

QPixmap IconFactory::pixmap(const QString& name, int size, const QColor& color, qreal devicePixelRatio)
{
    const int pixels = qMax(1, qRound(size * devicePixelRatio));
    QPixmap result(pixels, pixels);
    result.setDevicePixelRatio(devicePixelRatio);
    result.fill(Qt::transparent);

    QPainter painter(&result);
    painter.setRenderHint(QPainter::Antialiasing, true);
    paint(painter, name, QRectF(0, 0, size, size), color, 1.7);
    painter.end();
    return result;
}

QIcon IconFactory::icon(const QString& name, int size, const QColor& color)
{
    const QColor resolved = color.isValid()
        ? color
        : ThemeManager::instance().tokens().color.textPrimary;
    const QString key = QStringLiteral("%1|%2|%3|%4")
                            .arg(name)
                            .arg(size)
                            .arg(resolved.rgba())
                            .arg(ThemeManager::instance().isDark() ? 1 : 0);

    static QHash<QString, QIcon> cache;
    const auto found = cache.constFind(key);
    if (found != cache.constEnd()) {
        return found.value();
    }

    if (cache.size() > 512) {
        cache.clear();
    }

    const QIcon result = buildIcon(name, size, resolved, 1.7);
    cache.insert(key, result);
    return result;
}

QString IconFactory::glyphForApp(const QString& appId)
{
    if (appId == QStringLiteral("aero-finder")) return QStringLiteral("folder");
    if (appId == QStringLiteral("aero-settings") || appId == QStringLiteral("settings")) return QStringLiteral("gear");
    if (appId == QStringLiteral("aero-terminal")) return QStringLiteral("terminal");
    if (appId == QStringLiteral("aero-monitor")) return QStringLiteral("monitor");
    if (appId == QStringLiteral("aero-calculator")) return QStringLiteral("calculator");
    if (appId == QStringLiteral("aero-calendar")) return QStringLiteral("calendar");
    if (appId == QStringLiteral("aero-editor")) return QStringLiteral("file-text");
    if (appId == QStringLiteral("aero-tasks")) return QStringLiteral("activity");
    if (appId == QStringLiteral("aero-archive")) return QStringLiteral("archive");
    if (appId == QStringLiteral("aero-about")) return QStringLiteral("info");
    return QStringLiteral("apps");
}

QColor IconFactory::appColor(const QString& appId)
{
    if (appId == QStringLiteral("aero-finder")) return QColor(0x3D, 0x8B, 0xFF);
    if (appId == QStringLiteral("aero-settings")) return QColor(0x7B, 0x6F, 0xA8);
    if (appId == QStringLiteral("aero-terminal")) return QColor(0x2A, 0x24, 0x40);
    if (appId == QStringLiteral("aero-monitor")) return QColor(0x12, 0xB5, 0xA8);
    if (appId == QStringLiteral("aero-calculator")) return QColor(0xFF, 0x9B, 0x42);
    if (appId == QStringLiteral("aero-calendar")) return QColor(0xFF, 0x5D, 0x6C);
    if (appId == QStringLiteral("aero-editor")) return QColor(0x7C, 0x5C, 0xFF);
    if (appId == QStringLiteral("aero-tasks")) return QColor(0x32, 0xC9, 0x84);
    if (appId == QStringLiteral("aero-archive")) return QColor(0xE8, 0xA5, 0x2E);
    if (appId == QStringLiteral("aero-about")) return QColor(0x9B, 0x7B, 0xFF);
    return QColor(0x7C, 0x5C, 0xFF);
}

QIcon IconFactory::appIcon(const QString& appId, int size)
{
    constexpr qreal ratio = 2.0;
    QIcon icon;
    for (qreal scale : {1.0, ratio}) {
        const int pixels = qMax(1, qRound(size * scale));
        QPixmap pixmap(pixels, pixels);
        pixmap.setDevicePixelRatio(scale);
        pixmap.fill(Qt::transparent);

        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing, true);

        const QRectF tile(0.5, 0.5, size - 1.0, size - 1.0);
        const QColor base = appColor(appId);
        const QColor top = Core::Utils::mix(base, QColor(255, 255, 255), 0.24);
        const QColor bottom = Core::Utils::mix(base, QColor(0, 0, 0), 0.18);

        QLinearGradient gradient(tile.topLeft(), tile.bottomRight());
        gradient.setColorAt(0.0, top);
        gradient.setColorAt(1.0, bottom);

        QPainterPath shape;
        shape.addRoundedRect(tile.adjusted(0.5, 0.5, -0.5, -0.5), size * 0.235, size * 0.235);

        painter.setPen(Qt::NoPen);
        painter.setBrush(gradient);
        painter.drawPath(shape);

        painter.setPen(QPen(QColor(255, 255, 255, 70), 1.0));
        painter.setBrush(Qt::NoBrush);
        QPainterPath borderShape;
        borderShape.addRoundedRect(tile.adjusted(1.0, 1.0, -1.0, -1.0),
                                   size * 0.22, size * 0.22);
        painter.drawPath(borderShape);

        QLinearGradient gloss(tile.topLeft(), QPointF(tile.left(), tile.center().y()));
        gloss.setColorAt(0.0, QColor(255, 255, 255, 48));
        gloss.setColorAt(1.0, QColor(255, 255, 255, 0));
        painter.setPen(Qt::NoPen);
        painter.setBrush(gloss);
        QPainterPath upper;
        upper.addRoundedRect(QRectF(tile.left() + 1, tile.top() + 1, tile.width() - 2, tile.height() * 0.46),
                             size * 0.22, size * 0.22);
        painter.drawPath(upper);

        const qreal inset = size * 0.23;
        paint(painter, glyphForApp(appId),
             QRectF(inset, inset, size - inset * 2, size - inset * 2),
             QColor(255, 255, 255, 244),
             qMax(1.4, size / 26.0));

        painter.end();
        icon.addPixmap(pixmap);
    }
    return icon;
}

QStringList IconFactory::names()
{
    return {
        QStringLiteral("search"), QStringLiteral("wifi"), QStringLiteral("wifi-off"),
        QStringLiteral("bluetooth"), QStringLiteral("volume"), QStringLiteral("volume-muted"),
        QStringLiteral("battery"), QStringLiteral("battery-charging"), QStringLiteral("bell"),
        QStringLiteral("bell-off"), QStringLiteral("moon"), QStringLiteral("sun"),
        QStringLiteral("gear"), QStringLiteral("chevron-left"), QStringLiteral("chevron-right"),
        QStringLiteral("chevron-down"), QStringLiteral("chevron-up"), QStringLiteral("close"),
        QStringLiteral("minus"), QStringLiteral("square"), QStringLiteral("power"),
        QStringLiteral("lock"), QStringLiteral("user"), QStringLiteral("display"),
        QStringLiteral("keyboard"), QStringLiteral("mouse"), QStringLiteral("accessibility"),
        QStringLiteral("palette"), QStringLiteral("wallpaper"), QStringLiteral("dock"),
        QStringLiteral("desktop"), QStringLiteral("apps"), QStringLiteral("info"),
        QStringLiteral("plus"), QStringLiteral("check"), QStringLiteral("folder"),
        QStringLiteral("file"), QStringLiteral("file-text"), QStringLiteral("file-pdf"),
        QStringLiteral("image"), QStringLiteral("audio"), QStringLiteral("video"),
        QStringLiteral("archive"), QStringLiteral("home"), QStringLiteral("documents"),
        QStringLiteral("downloads"), QStringLiteral("pictures"), QStringLiteral("music"),
        QStringLiteral("trash"), QStringLiteral("back"), QStringLiteral("forward"),
        QStringLiteral("refresh"), QStringLiteral("play"), QStringLiteral("pause"),
        QStringLiteral("calculator"), QStringLiteral("calendar"), QStringLiteral("terminal"),
        QStringLiteral("monitor"), QStringLiteral("activity"), QStringLiteral("edit"),
        QStringLiteral("save"), QStringLiteral("copy"), QStringLiteral("cut"),
        QStringLiteral("paste"), QStringLiteral("undo"), QStringLiteral("redo"),
        QStringLiteral("grid"), QStringLiteral("list"), QStringLiteral("ellipsis"),
        QStringLiteral("arrow-up"), QStringLiteral("external"), QStringLiteral("eject"),
        QStringLiteral("star"), QStringLiteral("clock"), QStringLiteral("globe"),
        QStringLiteral("shield"), QStringLiteral("command"),
    };
}

} // namespace Aero::Theme
