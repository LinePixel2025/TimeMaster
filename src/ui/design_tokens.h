#pragma once

#include <QColor>
#include <QFont>
#include <QString>

#include <cmath>

#include "ui/theme_manager.h"

namespace DesignTokens {

// ============== THEME HELPER ==============
inline bool isDarkTheme() {
    return ThemeManager::instance()->isDark();
}

// ============== COLORS ==============
// 日晷仪表：墨绿底 + 单一玉色强调。禁止再引入靛蓝看板色。
inline QColor kBg()           { return isDarkTheme() ? QColor("#101614") : QColor("#F3F6F4"); }
inline QColor kSurface()      { return isDarkTheme() ? QColor("#1B2321") : QColor("#FFFFFF"); }
inline QColor kBorder()       { return isDarkTheme() ? QColor("#2C3633") : QColor("#D7E0DC"); }
inline QColor kAccent()       { return isDarkTheme() ? QColor("#3DCFB0") : QColor("#0B7A66"); }
inline QColor kAccentLight()  { return isDarkTheme() ? QColor("#16352E") : QColor("#D8EFE8"); }
inline QColor kAccentHover()  { return isDarkTheme() ? QColor("#5FDBBF") : QColor("#096655"); }
inline QColor kAccentPressed(){ return isDarkTheme() ? QColor("#8AE6CE") : QColor("#075244"); }
inline QColor kAccentGlow()   { return isDarkTheme() ? QColor(61,207,176,28) : QColor(11,122,102,22); }
inline QColor kSuccess()      { return isDarkTheme() ? QColor("#34D399") : QColor("#10B981"); }
inline QColor kError()        { return isDarkTheme() ? QColor("#F87171") : QColor("#EF4444"); }
inline QColor kAmber()        { return isDarkTheme() ? QColor("#E4B56A") : QColor("#B7812E"); }
inline QColor kTextStrong()   { return isDarkTheme() ? QColor("#F1F5F3") : QColor("#111827"); }
inline QColor kText()         { return isDarkTheme() ? QColor("#C5D0CB") : QColor("#374151"); }
inline QColor kTextMute()     { return isDarkTheme() ? QColor("#8A9993") : QColor("#6B7280"); }
inline QColor kTextFaint()    { return isDarkTheme() ? QColor("#5C6B66") : QColor("#9CA3AF"); }
inline QColor kOnAccent()     { return isDarkTheme() ? kBg() : QColor(Qt::white); }
inline QColor kFocusBorder()  { return isDarkTheme() ? kAccentHover() : kAccent(); }
inline QColor kTextPlaceholder() { return kTextMute(); }
inline QColor kChartValueText()  { return kTextMute(); }

inline QColor kCardBorder()          { return kBorder(); }
inline QColor kButtonHoverBg()       { return isDarkTheme() ? QColor("#24302C") : QColor("#E7EEEB"); }
inline QColor kChartGradientTop()    { return isDarkTheme() ? QColor("#5FDBBF") : QColor("#35B99A"); }
inline QColor kChartGradientBottom() { return isDarkTheme() ? QColor("#0B7A66") : QColor("#0B7A66"); }
inline QColor kChartAreaTop()        { return isDarkTheme() ? QColor(61,207,176,45) : QColor(53,185,154,55); }
inline QColor kChartAreaBottom()     { return isDarkTheme() ? QColor(61,207,176,0) : QColor(53,185,154,0); }
inline QColor kTodayGlow()           { return isDarkTheme() ? QColor(61,207,176,45) : QColor(11,122,102,35); }

inline qreal relativeLuminance(const QColor &color)
{
    const auto channel = [](qreal value) {
        value /= 255.0;
        return value <= 0.04045 ? value / 12.92 : std::pow((value + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * channel(color.redF() * 255.0)
         + 0.7152 * channel(color.greenF() * 255.0)
         + 0.0722 * channel(color.blueF() * 255.0);
}

inline QColor readableTextOn(const QColor &background)
{
    const QColor darkText("#111827");
    const QColor lightText(Qt::white);
    const qreal backgroundLuminance = relativeLuminance(background);
    const auto contrast = [backgroundLuminance](const QColor &foreground) {
        const qreal foregroundLuminance = relativeLuminance(foreground);
        return (qMax(backgroundLuminance, foregroundLuminance) + 0.05)
             / (qMin(backgroundLuminance, foregroundLuminance) + 0.05);
    };
    return contrast(darkText) >= contrast(lightText) ? darkText : lightText;
}

/// 热力图色阶：level 0（空）→ 4（最强），明暗两套各 5 档离散色。
inline QColor heatLevel(int level)
{
    level = qBound(0, level, 4);
    if (isDarkTheme()) {
        static const QColor levels[5] = {
            QColor("#24302C"), QColor("#1C302B"), QColor("#0B7A66"),
            QColor("#15977B"), QColor("#3DCFB0")
        };
        return levels[level];
    }
    static const QColor levels[5] = {
        QColor("#EDF3F0"), QColor("#D9EEE7"), QColor("#7FD4BE"),
        QColor("#35B99A"), QColor("#0B7A66")
    };
    return levels[level];
}
inline QColor kTodayDotBg()          { return kSurface(); }
inline QColor kProgressBg()          { return isDarkTheme() ? QColor("#2C3633") : QColor("#E2EAE7"); }
inline QColor kCompareTodayBg()      { return isDarkTheme() ? QColor("#16352E") : QColor("#E8F5F1"); }
inline QColor kCompareYesterdayBg()  { return isDarkTheme() ? QColor(255,255,255,2) : QColor(0,0,0,4); }
inline QColor kCompareYesterdayBar() { return isDarkTheme() ? QColor("#475569") : QColor("#D1D5DB"); }
inline QColor kPlaceholderIcon()     { return isDarkTheme() ? QColor("#5C6B66") : QColor("#9CA3AF"); }
inline QColor kPlaceholderBg()       { return isDarkTheme() ? QColor("#24302C") : QColor("#E5E7EB"); }
inline QColor kSeparator()           { return isDarkTheme() ? QColor(255,255,255,8) : QColor(0,0,0,10); }
inline QColor kTabInactiveBg()       { return QColor(Qt::transparent); }
inline QColor kTabInactiveText()     { return isDarkTheme() ? QColor("#8A9993") : QColor("#4B5563"); }

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

/// 数字/时长专用等宽字体，中文回退到雅黑。
inline QFont monoFont(int size, QFont::Weight weight = QFont::Normal) {
    QFont font;
    font.setFamilies({QStringLiteral("Cascadia Mono"),
                      QStringLiteral("Bahnschrift"),
                      QStringLiteral("Segoe UI Variable"),
                      QStringLiteral("Microsoft YaHei"),
                      QStringLiteral("monospace")});
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
inline constexpr int kRadiusCard  = 10;
inline constexpr int kRadiusBtn   = 6;
inline constexpr int kRadiusInput = 6;
inline constexpr int kRadiusChip  = 999;

// ============== LAYOUT ==============
inline constexpr int kOuterMargin = kSpacingXl;
inline constexpr int kWindowTopMargin = kSpacingLg;
inline constexpr int kWindowBottomMargin = 22;
inline constexpr int kSectionSpacing = kSpacingLg;
inline constexpr int kHeaderSpacing = kSpacingSm;
inline constexpr int kTitleStackSpacing = 1;
inline constexpr int kGridSpacing = kSpacingMd;
inline constexpr int kCardPaddingHorizontal = 20;
inline constexpr int kCardPaddingTop = 18;
inline constexpr int kCardPaddingBottom = 20;
inline constexpr int kCardContentSpacing = kSpacingMd;
inline constexpr int kCompactGap = kSpacingXs;
inline constexpr int kControlGap = kSpacingSm;
inline constexpr int kIconButtonSize = 36;
inline constexpr int kToggleButtonHeight = 26;
inline constexpr int kActionButtonMinHeight = 32;
inline constexpr int kHeroCompactBreakpoint = 740;
inline constexpr int kAiCompactBreakpoint = 740;
inline constexpr int kHeroMinHeightWide = 176;
inline constexpr int kHeroMinHeightCompact = 238;
inline constexpr int kTrendMinHeightNormal = 216;
inline constexpr int kTrendMinHeightMonth = 286;
inline constexpr int kRankVisibleRows = 5;
inline constexpr int kRankRowHeight = 32;
inline constexpr int kRankRowSpacing = 4;
inline constexpr int kRankProgressHeight = 4;
inline constexpr int kRankListHeight = kRankVisibleRows * kRankRowHeight
    + (kRankVisibleRows - 1) * kRankRowSpacing;
inline constexpr int kRankMinHeight = 248;
inline constexpr int kAiMinHeightWide = 148;
inline constexpr int kAiMinHeightCompact = 244;
inline constexpr int kStatusChipMaxWidth = 240;
inline constexpr int kStatusChipPaddingH = 10;
inline constexpr int kStatusChipPaddingV = 4;
inline constexpr int kCardSpacing = kSectionSpacing;
inline constexpr int kGridColumns = 2;

} // namespace DesignTokens
