#pragma once
#include <QColor>
#include <QFont>
#include <QString>
#include "ui/theme_manager.h"

namespace DesignTokens {

// ============== THEME HELPER ==============
inline bool isDarkTheme() {
    return ThemeManager::instance()->isDark();
}

// ============== COLORS ==============
inline QColor kBg()           { return isDarkTheme() ? QColor("#1E1E2E") : QColor("#F8F9FB"); }
inline QColor kSurface()      { return isDarkTheme() ? QColor("#2D2D3F") : QColor("#FFFFFF"); }
inline QColor kBorder()       { return isDarkTheme() ? QColor(255,255,255,8) : QColor(0,0,0,8); }
inline QColor kAccent()       { return isDarkTheme() ? QColor("#818CF8") : QColor("#6366F1"); }
inline QColor kAccentLight()  { return isDarkTheme() ? QColor("#6366F1") : QColor("#A5B4FC"); }
inline QColor kAccentHover()  { return isDarkTheme() ? QColor("#A5B4FC") : QColor("#4F46E5"); }
inline QColor kAccentPressed(){ return isDarkTheme() ? QColor("#C7D2FE") : QColor("#4338CA"); }
inline QColor kAccentGlow()   { return isDarkTheme() ? QColor(129,140,248,25) : QColor(99,102,241,20); }
inline QColor kSuccess()      { return isDarkTheme() ? QColor("#34D399") : QColor("#10B981"); }
inline QColor kError()        { return isDarkTheme() ? QColor("#F87171") : QColor("#EF4444"); }
inline QColor kTextStrong()   { return isDarkTheme() ? QColor("#F1F5F9") : QColor("#111827"); }
inline QColor kText()         { return isDarkTheme() ? QColor("#CBD5E1") : QColor("#374151"); }
inline QColor kTextMute()     { return isDarkTheme() ? QColor("#94A3B8") : QColor("#6B7280"); }
inline QColor kTextFaint()    { return isDarkTheme() ? QColor("#64748B") : QColor("#9CA3AF"); }

// ============== NEW TOKENS (formerly hardcoded) ==============
inline QColor kCardBorder()          { return isDarkTheme() ? QColor(255,255,255,10) : QColor(0,0,0,20); }
inline QColor kButtonHoverBg()       { return isDarkTheme() ? QColor(255,255,255,0.08*255) : QColor("#E5E7EB"); }
inline QColor kChartGradientTop()    { return isDarkTheme() ? QColor("#6366F1") : QColor("#A5B4FC"); }
inline QColor kChartGradientBottom() { return isDarkTheme() ? QColor("#4338CA") : QColor("#6366F1"); }
inline QColor kChartAreaTop()        { return isDarkTheme() ? QColor(99,102,241,40) : QColor(165,180,252,60); }
inline QColor kChartAreaBottom()     { return isDarkTheme() ? QColor(99,102,241,0) : QColor(165,180,252,0); }
inline QColor kTodayGlow()           { return isDarkTheme() ? QColor(129,140,248,45) : QColor(99,102,241,38); }
inline QColor kTodayDotBg()          { return isDarkTheme() ? QColor("#2D2D3F") : QColor("#FFFFFF"); }
inline QColor kProgressBg()          { return isDarkTheme() ? QColor(255,255,255,0.12*255) : QColor("#E5E7EB"); }
inline QColor kCompareTodayBg()      { return isDarkTheme() ? QColor(129,140,248,25) : QColor(99,102,241,20); }
inline QColor kCompareYesterdayBg()  { return isDarkTheme() ? QColor(255,255,255,2) : QColor(0,0,0,4); }
inline QColor kCompareYesterdayBar() { return isDarkTheme() ? QColor("#475569") : QColor("#D1D5DB"); }
inline QColor kPlaceholderIcon()     { return isDarkTheme() ? QColor("#64748B") : QColor("#9CA3AF"); }
inline QColor kPlaceholderBg()       { return isDarkTheme() ? QColor("#334155") : QColor("#E5E7EB"); }
inline QColor kSeparator()           { return isDarkTheme() ? QColor(255,255,255,6) : QColor(0,0,0,10); }
inline QColor kTabInactiveBg()       { return QColor(Qt::transparent); }
inline QColor kTabInactiveText()     { return isDarkTheme() ? QColor("#94A3B8") : QColor("#4B5563"); }

// ============== WALLPAPER ==============
inline QColor kWhiteBg()    { return isDarkTheme() ? QColor("#2D2D3F") : QColor("#FFFFFF"); }
inline QColor kFrostStart() { return isDarkTheme() ? QColor("#252540") : QColor("#EFF2F9"); }
inline QColor kFrostEnd()   { return isDarkTheme() ? QColor("#2A2535") : QColor("#F5F0EC"); }
inline constexpr const char* kWallpaperKey = "wallpaper";

// ============== TYPOGRAPHY ==============
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
