#include "aero/graphics/backdrop_provider.h"

#include <QImage>
#include <QPixmap>
#include <QWidget>

#include <algorithm>
#include <vector>

namespace Aero::Graphics {
namespace {

void boxBlurHorizontal(const QImage& source, QImage& target, int radius)
{
    const int width = source.width();
    const int height = source.height();
    const int window = radius * 2 + 1;

    for (int y = 0; y < height; ++y) {
        const auto* src = reinterpret_cast<const QRgb*>(source.constScanLine(y));
        auto* dst = reinterpret_cast<QRgb*>(target.scanLine(y));

        qint64 a = 0;
        qint64 r = 0;
        qint64 g = 0;
        qint64 b = 0;

        for (int x = -radius; x <= radius; ++x) {
            const QRgb pixel = src[std::clamp(x, 0, width - 1)];
            a += qAlpha(pixel);
            r += qRed(pixel);
            g += qGreen(pixel);
            b += qBlue(pixel);
        }

        for (int x = 0; x < width; ++x) {
            dst[x] = qRgba(static_cast<int>(r / window),
                           static_cast<int>(g / window),
                           static_cast<int>(b / window),
                           static_cast<int>(a / window));

            const QRgb outgoing = src[std::clamp(x - radius, 0, width - 1)];
            const QRgb incoming = src[std::clamp(x + radius + 1, 0, width - 1)];
            a += qAlpha(incoming) - qAlpha(outgoing);
            r += qRed(incoming) - qRed(outgoing);
            g += qGreen(incoming) - qGreen(outgoing);
            b += qBlue(incoming) - qBlue(outgoing);
        }
    }
}

void boxBlurVertical(const QImage& source, QImage& target, int radius)
{
    const int width = source.width();
    const int height = source.height();
    const int window = radius * 2 + 1;

    for (int x = 0; x < width; ++x) {
        qint64 a = 0;
        qint64 r = 0;
        qint64 g = 0;
        qint64 b = 0;

        for (int y = -radius; y <= radius; ++y) {
            const QRgb pixel = source.pixel(x, std::clamp(y, 0, height - 1));
            a += qAlpha(pixel);
            r += qRed(pixel);
            g += qGreen(pixel);
            b += qBlue(pixel);
        }

        for (int y = 0; y < height; ++y) {
            target.setPixel(x, y,
                            qRgba(static_cast<int>(r / window),
                                  static_cast<int>(g / window),
                                  static_cast<int>(b / window),
                                  static_cast<int>(a / window)));

            const QRgb outgoing = source.pixel(x, std::clamp(y - radius, 0, height - 1));
            const QRgb incoming = source.pixel(x, std::clamp(y + radius + 1, 0, height - 1));
            a += qAlpha(incoming) - qAlpha(outgoing);
            r += qRed(incoming) - qRed(outgoing);
            g += qGreen(incoming) - qGreen(outgoing);
            b += qBlue(incoming) - qBlue(outgoing);
        }
    }
}

} // namespace

QImage BackdropProvider::blurImage(const QImage& source, int radius)
{
    if (source.isNull() || radius <= 0) {
        return source;
    }

    const int clampedRadius = std::clamp(radius, 1, 32);
    QImage current = source.convertToFormat(QImage::Format_ARGB32);
    QImage temporary(current.size(), QImage::Format_ARGB32);
    temporary.setDevicePixelRatio(1.0);

    // Two box passes approximate a Gaussian closely enough for frosted glass
    // and cost far less than a true Gaussian convolution.
    for (int pass = 0; pass < 2; ++pass) {
        boxBlurHorizontal(current, temporary, clampedRadius);
        boxBlurVertical(temporary, current, clampedRadius);
    }

    return current;
}

QPixmap BackdropProvider::capture(QWidget* root, int blurRadius)
{
    if (root == nullptr || root->size().isEmpty()) {
        return {};
    }

    const QPixmap shot = root->grab();
    if (shot.isNull()) {
        return {};
    }

    const QSize logical = root->size();
    const QImage source = shot.toImage().convertToFormat(QImage::Format_ARGB32);
    const QSize small(std::max(1, logical.width() / 4), std::max(1, logical.height() / 4));

    QImage downscaled = source.scaled(small, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    downscaled = blurredPixmap(QPixmap::fromImage(downscaled), qMax(2, blurRadius / 4), small).toImage();

    QImage result = downscaled.scaled(logical, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    result.setDevicePixelRatio(1.0);
    return QPixmap::fromImage(result);
}

QPixmap BackdropProvider::blurredPixmap(const QPixmap& source, int radius, const QSize& logicalSize)
{
    if (source.isNull()) {
        return {};
    }
    QImage image = source.toImage().convertToFormat(QImage::Format_ARGB32);
    image = blurImage(image, radius);
    image.setDevicePixelRatio(1.0);
    if (logicalSize.isValid() && image.size() != logicalSize) {
        image = image.scaled(logicalSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }
    return QPixmap::fromImage(image);
}

} // namespace Aero::Graphics
