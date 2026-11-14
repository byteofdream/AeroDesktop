#pragma once

#include <QColor>
#include <QRectF>
#include <QString>
#include <QStringList>

class QPainter;

namespace Aero::Graphics::Wallpaper {

/// Built-in, resolution-independent wallpapers.  They are painted with
/// gradients and soft radial glows so Aero ships without binary assets and
/// looks correct at any scale factor.
QStringList presetIds();
QString displayName(const QString& id);
QColor accentColor(const QString& id);

void paintPreset(QPainter& painter,
                 const QRectF& rect,
                 const QString& id,
                 bool dark,
                 bool withGlow = true);

} // namespace Aero::Graphics::Wallpaper
