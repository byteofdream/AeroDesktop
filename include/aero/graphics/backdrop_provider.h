#pragma once

#include <QImage>
#include <QPixmap>

class QWidget;

namespace Aero::Graphics {

/// Captures the desktop, downsamples it, applies a fast separable box blur and
/// returns a device-independent pixmap that glass surfaces can sample.
///
/// This is deliberately snapshot based: Qt Widgets has no cross-platform
/// backdrop-filter primitive.  The shell refreshes the snapshot only when an
/// overlay opens or a menu appears, keeping idle cost at zero.  When blur is
/// disabled glass panels fall back to a translucent tint with no sampling.
class BackdropProvider {
public:
    /// @param blurRadius approximate Gaussian radius in logical pixels.
    static QPixmap capture(QWidget* root, int blurRadius = 28);

    static QImage blurImage(const QImage& source, int radius);
    static QPixmap blurredPixmap(const QPixmap& source, int radius, const QSize& logicalSize);

private:
    BackdropProvider() = delete;
};

} // namespace Aero::Graphics
