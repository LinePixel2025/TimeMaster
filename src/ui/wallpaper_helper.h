#pragma once
#include <QPixmap>
#include <QString>

namespace WallpaperHelper {

enum class Type { White, FrostedGlass };

inline QString toString(Type t) {
    switch (t) {
    case Type::White:         return QStringLiteral("white");
    case Type::FrostedGlass:  return QStringLiteral("frosted_glass");
    }
    return QStringLiteral("white");
}

inline Type fromString(const QString &s) {
    if (s == QStringLiteral("frosted_glass"))
        return Type::FrostedGlass;
    return Type::White;
}

QPixmap generateWallpaper(Type type, const QSize &size);

} // namespace WallpaperHelper
