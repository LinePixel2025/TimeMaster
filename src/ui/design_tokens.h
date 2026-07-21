#pragma once
#include <QColor>
#include <QFont>
#include <QString>

namespace DesignTokens {

// ============== COLORS ==============
inline const QColor kBg           = QColor("#F8F9FB");
inline const QColor kSurface      = QColor("#FFFFFF");
inline const QColor kBorder       = QColor(  0,   0,   0,   8);
inline const QColor kAccent       = QColor("#6366F1");
inline const QColor kAccentLight  = QColor("#A5B4FC");
inline const QColor kAccentHover  = QColor("#4F46E5");
inline const QColor kAccentPressed= QColor("#4338CA");
inline const QColor kAccentGlow   = QColor( 99, 102, 241,  20);
inline const QColor kSuccess      = QColor("#10B981");
inline const QColor kError        = QColor("#EF4444");
inline const QColor kTextStrong   = QColor("#111827");
inline const QColor kText         = QColor("#374151");
inline const QColor kTextMute     = QColor("#6B7280");
inline const QColor kTextFaint    = QColor("#9CA3AF");

// ============== WALLPAPER ==============
inline const QColor kWhiteBg    = QColor("#FFFFFF");
inline const QColor kFrostStart = QColor("#EFF2F9");  // cool light blue-gray
inline const QColor kFrostEnd   = QColor("#F5F0EC");  // warm light beige
inline constexpr const char* kWallpaperKey = "wallpaper";

// ============== TYPOGRAPHY ==============
// Microsoft YaHei — Mandatory CJK font for this project.
// Uses setPointSize for DPI-free rendering.
inline QFont appFont(int size, QFont::Weight weight = QFont::Normal) {
    QFont font;
    font.setFamilies({QStringLiteral("Microsoft YaHei"),
                      QStringLiteral("Microsoft YaHei UI"),
                      QStringLiteral("SimHei"),
                      QStringLiteral("DengXian"),
                      QStringLiteral("sans-serif")});
    font.setPointSize(size);
    font.setWeight(weight);
    font.setStyleStrategy(QFont::PreferAntialias);
    return font;
}

inline QFont eyebrowFont(int size = 11) {
    QFont f = appFont(size, QFont::Medium);
    f.setLetterSpacing(QFont::AbsoluteSpacing, 0.8);
    return f;
}

// ============== SPACING (8pt scale) ==============
inline constexpr int kSpacingXs  = 4;
inline constexpr int kSpacingSm  = 8;
inline constexpr int kSpacingMd  = 12;
inline constexpr int kSpacingLg  = 16;
inline constexpr int kSpacingXl  = 24;
inline constexpr int kSpacing2xl = 32;
inline constexpr int kSpacing3xl = 48;

// ============== RADII ==============
inline constexpr int kRadiusCard  = 16;
inline constexpr int kRadiusBtn   = 10;
inline constexpr int kRadiusInput = 8;
inline constexpr int kRadiusChip  = 999;

// ============== LAYOUT ==============
inline constexpr int kOuterMargin = 24;
inline constexpr int kCardSpacing = 16;
inline constexpr int kGridColumns = 2;

} // namespace DesignTokens
