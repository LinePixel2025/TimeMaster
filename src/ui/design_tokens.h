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

// ============== COLOR TOOLS ==============
inline QColor mixColors(const QColor &a, const QColor &b, qreal t)
{
    const qreal k = qBound(0.0, t, 1.0);
    return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * k,
                            a.greenF() + (b.greenF() - a.greenF()) * k,
                            a.blueF() + (b.blueF() - a.blueF()) * k);
}

inline QColor shiftedLightness(const QColor &color, qreal delta)
{
    float h, s, l, a;
    color.getHslF(&h, &s, &l, &a);
    QColor result = color;
    result.setHslF(h, s, qBound(0.0f, static_cast<float>(l + delta), 1.0f), a);
    return result;
}

inline QColor scaledSaturation(const QColor &color, qreal factor)
{
    float h, s, l, a;
    color.getHslF(&h, &s, &l, &a);
    QColor result = color;
    result.setHslF(h, qBound(0.0f, static_cast<float>(s * factor), 1.0f), l, a);
    return result;
}

namespace accent_detail {

// brand 色：用户选择的主题色；未自定义时为默认玉色（仅用于派生计算）。
inline QColor brandAccent()
{
    const QColor custom = ThemeManager::instance()->accentColor();
    return custom.isValid() ? custom : ThemeManager::defaultAccent();
}

// 亮色主题档：保证足够深以承载白字。
inline QColor deepAccent()
{
    QColor c = brandAccent();
    float h, s, l, a;
    c.getHslF(&h, &s, &l, &a);
    if (l > 0.45f)
        c.setHslF(h, s, 0.45f, a);
    return c;
}

// 暗色主题档：保证足够亮以承载深色文字。
inline QColor brightAccent()
{
    QColor c = brandAccent();
    float h, s, l, a;
    c.getHslF(&h, &s, &l, &a);
    if (l < 0.55f)
        c.setHslF(h, s, 0.55f, a);
    return c;
}

} // namespace accent_detail

/// 主题色 token 取值：未自定义主题色时返回内置默认（默认视觉零变化），
/// 自定义后调用 derive(dark) 按品牌色派生。
template <typename Derive>
inline QColor accentToken(QColor defaultDark, QColor defaultLight, Derive derive)
{
    if (!ThemeManager::instance()->hasCustomAccent())
        return isDarkTheme() ? defaultDark : defaultLight;
    return derive(isDarkTheme());
}

// ============== COLORS ==============
// 单一强调色：默认玉色，可在设置中自定义；禁止再引入靛蓝看板色。
inline QColor kBg()           { return isDarkTheme() ? QColor("#101614") : QColor("#F3F6F4"); }
inline QColor kSurface()      { return isDarkTheme() ? QColor("#1B2321") : QColor("#FFFFFF"); }
inline QColor kBorder()       { return isDarkTheme() ? QColor("#2C3633") : QColor("#D7E0DC"); }
inline QColor kAccent()       { return accentToken(QColor("#3DCFB0"), QColor("#0B7A66"),
    [](bool dark) { return dark ? accent_detail::brightAccent() : accent_detail::deepAccent(); }); }
inline QColor kAccentLight()  { return accentToken(QColor("#16352E"), QColor("#D8EFE8"),
    [](bool dark) { return dark ? mixColors(kBg(), accent_detail::brightAccent(), 0.16)
                                : mixColors(QColor(Qt::white), accent_detail::deepAccent(), 0.15); }); }
inline QColor kAccentHover()  { return accentToken(QColor("#5FDBBF"), QColor("#096655"),
    [](bool dark) { return dark ? shiftedLightness(accent_detail::brightAccent(), 0.09)
                                : shiftedLightness(accent_detail::deepAccent(), -0.045); }); }
inline QColor kAccentPressed(){ return accentToken(QColor("#8AE6CE"), QColor("#075244"),
    [](bool dark) { return dark ? shiftedLightness(accent_detail::brightAccent(), 0.19)
                                : shiftedLightness(accent_detail::deepAccent(), -0.09); }); }
inline QColor kAccentGlow()   { return accentToken(QColor(61,207,176,28), QColor(11,122,102,22),
    [](bool dark) { QColor c = dark ? accent_detail::brightAccent() : accent_detail::deepAccent();
                    c.setAlpha(dark ? 28 : 22); return c; }); }
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
// 图表渐变：顶亮底深，亮色主题顶部提亮更多。
inline QColor kChartGradientTop()    { return accentToken(QColor("#5FDBBF"), QColor("#35B99A"),
    [](bool dark) { return dark ? shiftedLightness(accent_detail::brightAccent(), 0.09)
                                : shiftedLightness(accent_detail::deepAccent(), 0.20); }); }
inline QColor kChartGradientBottom() { return accentToken(QColor("#0B7A66"), QColor("#0B7A66"),
    [](bool) { return accent_detail::deepAccent(); }); }
inline QColor kChartAreaTop()        { return accentToken(QColor(61,207,176,45), QColor(53,185,154,55),
    [](bool dark) { QColor c = dark ? accent_detail::brightAccent()
                                    : shiftedLightness(accent_detail::deepAccent(), 0.20);
                    c.setAlpha(dark ? 45 : 55); return c; }); }
inline QColor kChartAreaBottom()     { return accentToken(QColor(61,207,176,0), QColor(53,185,154,0),
    [](bool dark) { QColor c = dark ? accent_detail::brightAccent()
                                    : shiftedLightness(accent_detail::deepAccent(), 0.20);
                    c.setAlpha(0); return c; }); }
inline QColor kTodayGlow()           { return accentToken(QColor(61,207,176,45), QColor(11,122,102,35),
    [](bool dark) { QColor c = dark ? accent_detail::brightAccent() : accent_detail::deepAccent();
                    c.setAlpha(dark ? 45 : 35); return c; }); }

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

// ============== ACCENT FOR REPORTS ==============
// 报告 HTML 同时生成亮/暗两套 CSS 变量，由浏览器 prefers-color-scheme 选择，
// 因此显式传 dark 参数，不读应用当前主题。
inline QColor accentFor(bool dark)
{
    return dark ? accent_detail::brightAccent() : accent_detail::deepAccent();
}

/// 报告用 accent-light（≈ kChartGradientTop 的显式亮/暗版本）。
inline QColor accentLightFor(bool dark)
{
    return dark ? shiftedLightness(accent_detail::brightAccent(), 0.09)
                : shiftedLightness(accent_detail::deepAccent(), 0.20);
}

/// 热力图色阶：level 0（空）→ 4（最强）。显式传 dark 供报告 HTML 生成亮/暗两套值；
/// 0 档为中性空档底色，不随主题色变化。
inline QColor heatLevelFor(bool dark, int level)
{
    level = qBound(0, level, 4);
    const QColor deep = accent_detail::deepAccent();
    const QColor bright = accent_detail::brightAccent();
    if (dark) {
        switch (level) {
        case 0:  return QColor("#24302C");
        case 1:  return mixColors(QColor("#101614"), bright, 0.15);
        case 2:  return deep;
        case 3:  return shiftedLightness(deep, 0.08);
        default: return bright;
        }
    }
    switch (level) {
    case 0:  return QColor("#EDF3F0");
    case 1:  return mixColors(QColor(Qt::white), deep, 0.14);
    case 2:  return scaledSaturation(shiftedLightness(deep, 0.40), 0.6);
    case 3:  return shiftedLightness(deep, 0.20);
    default: return deep;
    }
}

/// 热力图色阶：level 0（空）→ 4（最强），明暗两套各 5 档离散色。
inline QColor heatLevel(int level)
{
    level = qBound(0, level, 4);
    if (!ThemeManager::instance()->hasCustomAccent()) {
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
    return heatLevelFor(isDarkTheme(), level);
}
inline QColor kTodayDotBg()          { return kSurface(); }
inline QColor kProgressBg()          { return isDarkTheme() ? QColor("#2C3633") : QColor("#E2EAE7"); }
inline QColor kCompareTodayBg()      { return accentToken(QColor("#16352E"), QColor("#E8F5F1"),
    [](bool dark) { return dark ? mixColors(QColor("#101614"), accent_detail::brightAccent(), 0.16)
                                : mixColors(QColor(Qt::white), accent_detail::deepAccent(), 0.09); }); }
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
