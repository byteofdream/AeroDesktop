#include "aero/shell/wallpaper_view.h"

#include "aero/graphics/wallpaper.h"
#include "aero/settings/settings_keys.h"
#include "aero/settings/settings_manager.h"
#include "aero/theme/theme.h"

#include <QImageReader>
#include <QLinearGradient>
#include <QPainter>
#include <QResizeEvent>

namespace Aero::Shell {
namespace {

QPixmap loadWallpaperImage(const QString& path, const QSize& target, qreal ratio)
{
    QImageReader reader(path);
    reader.setAutoTransform(true);
    if (!reader.canRead()) {
        return {};
    }

    const QSize sourceSize = reader.size();
    if (sourceSize.isValid()) {
        QSize decodeSize(qMax(1, qRound(target.width() * ratio)),
                         qMax(1, qRound(target.height() * ratio)));
        if (sourceSize.width() > decodeSize.width() || sourceSize.height() > decodeSize.height()) {
            decodeSize = sourceSize.scaled(decodeSize, Qt::KeepAspectRatioByExpanding);
            reader.setScaledSize(decodeSize);
        }
    }

    QImage image = reader.read();
    if (image.isNull()) {
        return {};
    }

    QPixmap pixmap = QPixmap::fromImage(image);
    pixmap.setDevicePixelRatio(ratio);
    return pixmap;
}

} // namespace

WallpaperView::WallpaperView(Settings::SettingsManager* settings, QWidget* parent)
    : QWidget(parent)
    , m_settings(settings)
{
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setAttribute(Qt::WA_NoSystemBackground, false);
    setFocusPolicy(Qt::NoFocus);

    if (m_settings != nullptr) {
        connect(m_settings, &Settings::SettingsManager::settingChanged, this,
                [this](const QString& key, const QVariant&) {
                    if (key.startsWith(QStringLiteral("wallpaper."))) {
                        reload();
                    }
                });
    }

    connect(&Theme::ThemeManager::instance(), &Theme::ThemeManager::themeChanged,
            this, &WallpaperView::reload);
}

void WallpaperView::reload()
{
    m_cacheValid = false;
    m_cache = {};
    update();
}

QString WallpaperView::currentWallpaperId() const
{
    return m_settings != nullptr ? m_settings->wallpaperId() : QStringLiteral("aurora");
}

QSize WallpaperView::sizeHint() const
{
    return QSize(1440, 900);
}

void WallpaperView::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    if (width() <= 0 || height() <= 0) {
        return;
    }
    if (!m_cacheValid || m_cacheSize != size()) {
        rebuildCache();
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    if (!m_cache.isNull()) {
        painter.drawPixmap(rect(), m_cache, m_cache.rect());
    } else {
        const Theme::ThemeTokens& tokens = Theme::ThemeManager::instance().tokens();
        QLinearGradient fallback(rect().topLeft(), rect().bottomRight());
        fallback.setColorAt(0.0, tokens.color.wallpaperStart);
        fallback.setColorAt(0.55, tokens.color.wallpaperMid);
        fallback.setColorAt(1.0, tokens.color.wallpaperEnd);
        painter.fillRect(rect(), fallback);
    }

    // A restrained vignette keeps white window chrome and the dock legible on
    // bright photographic wallpapers without washing out the imagery.
    QLinearGradient vignette(rect().topLeft(), rect().bottomLeft());
    vignette.setColorAt(0.0, QColor(0, 0, 0, Theme::ThemeManager::instance().isDark() ? 34 : 8));
    vignette.setColorAt(0.42, QColor(0, 0, 0, 0));
    vignette.setColorAt(1.0, QColor(0, 0, 0, Theme::ThemeManager::instance().isDark() ? 46 : 16));
    painter.fillRect(rect(), vignette);
}

void WallpaperView::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    m_cacheValid = false;
}

void WallpaperView::rebuildCache()
{
    m_cacheSize = size();
    if (m_cacheSize.isEmpty()) {
        m_cacheValid = false;
        return;
    }

    const qreal ratio = devicePixelRatioF();
    const QString id = currentWallpaperId();
    const QString path = m_settings != nullptr ? m_settings->wallpaperPath() : QString();
    const QString fit = m_settings != nullptr ? m_settings->wallpaperFit() : QStringLiteral("fill");
    const bool dark = Theme::ThemeManager::instance().isDark();

    m_cachedId = id;
    m_cachedPath = path;
    m_cachedFit = fit;

    QPixmap result;
    if (!path.isEmpty()) {
        const QPixmap image = loadWallpaperImage(path, m_cacheSize, ratio);
        if (!image.isNull()) {
            const QSize pixelSize(qMax(1, qRound(m_cacheSize.width() * ratio)),
                                  qMax(1, qRound(m_cacheSize.height() * ratio)));
            result = QPixmap(pixelSize);
            result.setDevicePixelRatio(ratio);
            result.fill(Qt::black);

            QPainter painter(&result);
            painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
            const QRectF target = QRectF(QPointF(0, 0), QSizeF(m_cacheSize));
            const QSizeF imageSize(image.size());

            const QSizeF logicalImageSize = image.deviceIndependentSize();
            if (fit == QStringLiteral("fit")) {
                const QSizeF scaled = logicalImageSize.scaled(target.size(), Qt::KeepAspectRatio);
                const QRectF destination(target.center() - QPointF(scaled.width() / 2.0, scaled.height() / 2.0),
                                         scaled);
                painter.fillRect(target, QColor(8, 7, 12));
                painter.drawPixmap(destination, image, image.rect());
            } else if (fit == QStringLiteral("stretch")) {
                painter.drawPixmap(target, image, image.rect());
            } else if (fit == QStringLiteral("center")) {
                painter.fillRect(target, QColor(8, 7, 12));
                const QRectF destination(target.center() - QPointF(logicalImageSize.width() / 2.0,
                                                                   logicalImageSize.height() / 2.0),
                                         logicalImageSize);
                painter.drawPixmap(destination, image, image.rect());
            } else if (fit == QStringLiteral("tile")) {
                painter.fillRect(target, QColor(8, 7, 12));
                for (qreal y = 0; y < target.height(); y += logicalImageSize.height()) {
                    for (qreal x = 0; x < target.width(); x += logicalImageSize.width()) {
                        painter.drawPixmap(QRectF(QPointF(x, y), logicalImageSize), image, image.rect());
                    }
                }
            } else {
                const QSizeF targetPixels(target.width() * ratio, target.height() * ratio);
                QRectF crop = image.rect();
                const qreal targetAspect = targetPixels.width() / targetPixels.height();
                const qreal imageAspect = imageSize.width() / imageSize.height();
                if (imageAspect > targetAspect) {
                    const qreal width = imageSize.height() * targetAspect;
                    crop.setLeft((imageSize.width() - width) / 2.0);
                    crop.setWidth(width);
                } else if (imageAspect < targetAspect) {
                    const qreal height = imageSize.width() / targetAspect;
                    crop.setTop((imageSize.height() - height) / 2.0);
                    crop.setHeight(height);
                }
                painter.drawPixmap(target, image, crop);
            }
            painter.end();
        }
    }

    if (result.isNull()) {
        const QSize pixelSize(qMax(1, qRound(m_cacheSize.width() * ratio)),
                              qMax(1, qRound(m_cacheSize.height() * ratio)));
        result = QPixmap(pixelSize);
        result.setDevicePixelRatio(ratio);
        result.fill(Qt::transparent);
        QPainter painter(&result);
        painter.setRenderHint(QPainter::Antialiasing, true);
        Aero::Graphics::Wallpaper::paintPreset(painter,
                                               QRectF(QPointF(0, 0), QSizeF(m_cacheSize)),
                                               id.isEmpty() ? QStringLiteral("aurora") : id,
                                               dark);
        painter.end();
    }

    m_cache = result;
    m_cacheValid = true;
}

void WallpaperView::rebuildDesktopTexture()
{
    // Reserved for a future multi-monitor wallpaper span.  The current cache
    // already renders at the full desktop size, so there is nothing extra to
    // compose today.
}

} // namespace Aero::Shell
