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
inline QColor kBg()           { return isDarkTheme() ? QColor("#171C1B") : QColor("#F3F6F5"); }
inline QColor kSurface()      { return isDarkTheme() ? QColor("#222927") : QColor("#FFFFFF"); }
inline QColor kBorder()       { return isDarkTheme() ? QColor("#39423F") : QColor("#DCE4E1"); }
inline QColor kAccent()       { return isDarkTheme() ? QColor("#4DD6B0") : QColor("#087F6B"); }
inline QColor kAccentLight()  { return isDarkTheme() ? QColor("#183E35") : QColor("#D9F2EB"); }
inline QColor kAccentHover()  { return isDarkTheme() ? QColor("#72E2C2") : QColor("#066B5B"); }
inline QColor kAccentPressed(){ return isDarkTheme() ? QColor("#98EBD2") : QColor("#05584C"); }
inline QColor kAccentGlow()   { return isDarkTheme() ? QColor(77,214,176,28) : QColor(8,127,107,22); }
inline QColor kSuccess()      { return isDarkTheme() ? QColor("#34D399") : QColor("#10B981"); }
inline QColor kError()        { return isDarkTheme() ? QColor("#F87171") : QColor("#EF4444"); }
inline QColor kTextStrong()   { return isDarkTheme() ? QColor("#F1F5F9") : QColor("#111827"); }
inline QColor kText()         { return isDarkTheme() ? QColor("#CBD5E1") : QColor("#374151"); }
inline QColor kTextMute()     { return isDarkTheme() ? QColor("#94A3B8") : QColor("#6B7280"); }
inline QColor kTextFaint()    { return isDarkTheme() ? QColor("#64748B") : QColor("#9CA3AF"); }

// ============== NEW TOKENS (formerly hardcoded) ==============
inline QColor kCardBorder()          { return kBorder(); }
inline QColor kButtonHoverBg()       { return isDarkTheme() ? QColor("#303936") : QColor("#E7EEEB"); }
inline QColor kChartGradientTop()    { return isDarkTheme() ? QColor("#72E2C2") : QColor("#35B99A"); }
inline QColor kChartGradientBottom() { return isDarkTheme() ? QColor("#15977B") : QColor("#087F6B"); }
inline QColor kChartAreaTop()        { return isDarkTheme() ? QColor(77,214,176,45) : QColor(53,185,154,55); }
inline QColor kChartAreaBottom()     { return isDarkTheme() ? QColor(77,214,176,0) : QColor(53,185,154,0); }
inline QColor kTodayGlow()           { return isDarkTheme() ? QColor(77,214,176,45) : QColor(8,127,107,35); }
/// 热力图色阶：level 0（空）→ 4（最强），明暗两套各 5 档离散色，参考 GitHub 贡献图。
inline QColor heatLevel(int level)
{
    level = qBound(0, level, 4);
    if (isDarkTheme()) {
        static const QColor levels[5] = {
            QColor("#2E3A37"), QColor("#1B4A3E"), QColor("#0F6B56"),
            QColor("#15977B"), QColor("#4DD6B0")
        };
        return levels[level];
    }
    static const QColor levels[5] = {
        QColor("#E4ECE9"), QColor("#CDE9E0"), QColor("#7FD4BE"),
        QColor("#35B99A"), QColor("#087F6B")
    };
    return levels[level];
}
inline QColor kTodayDotBg()          { return kSurface(); }
inline QColor kProgressBg()          { return isDarkTheme() ? QColor("#3A4542") : QColor("#E2EAE7"); }
inline QColor kCompareTodayBg()      { return isDarkTheme() ? QColor("#193B33") : QColor("#E8F5F1"); }
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
inline constexpr int kRadiusCard  = 8;
inline constexpr int kRadiusBtn   = 6;
inline constexpr int kRadiusInput = 6;
inline constexpr int kRadiusChip  = 999;

// ============== LAYOUT ==============
inline constexpr int kOuterMargin = 24;
inline constexpr int kCardSpacing = 16;
inline constexpr int kGridColumns = 2;

} // namespace DesignTokens
