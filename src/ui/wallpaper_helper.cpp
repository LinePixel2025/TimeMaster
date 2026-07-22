#include "ui/wallpaper_helper.h"
#include "ui/design_tokens.h"
#include <QImage>
#include <QLinearGradient>
#include <QPainter>
#include <QRandomGenerator>

namespace WallpaperHelper {

QPixmap generateWallpaper(Type type, const QSize &size) {
    if (size.width() <= 0 || size.height() <= 0)
        return QPixmap();

    switch (type) {
    case Type::White: {
        QPixmap pixmap(size);
        pixmap.fill(DesignTokens::kWhiteBg());
        return pixmap;
    }
    case Type::FrostedGlass: {
        // Base gradient
        QPixmap pixmap(size);
        {
            QPainter painter(&pixmap);
            QLinearGradient gradient(0, 0, size.width(), size.height());
            gradient.setColorAt(0.0, DesignTokens::kFrostStart());
            gradient.setColorAt(1.0, DesignTokens::kFrostEnd());
            painter.fillRect(pixmap.rect(), gradient);
        }

        // Subtle noise overlay via per-pixel perturbation
        QImage image = pixmap.toImage();
        const int w = image.width();
        const int h = image.height();
        auto *rng = QRandomGenerator::global();

        for (int y = 0; y < h; ++y) {
            auto *scanLine = reinterpret_cast<QRgb *>(image.scanLine(y));
            for (int x = 0; x < w; ++x) {
                const QRgb pixel = scanLine[x];
                const auto clampChannel = [](int v) -> int {
                    return (v < 0) ? 0 : (v > 255 ? 255 : v);
                };
                const int r = clampChannel(qRed(pixel)   + rng->bounded(-3, 4));
                const int g = clampChannel(qGreen(pixel) + rng->bounded(-3, 4));
                const int b = clampChannel(qBlue(pixel)  + rng->bounded(-3, 4));
                scanLine[x] = qRgb(r, g, b);
            }
        }

        return QPixmap::fromImage(image);
    }
    }

    return QPixmap();
}

} // namespace WallpaperHelper
