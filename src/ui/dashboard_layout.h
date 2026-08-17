#pragma once

namespace DashboardLayout {

enum class Mode {
    SingleColumn,
    DualColumn
};

inline constexpr int kTrendMinWidth = 420;
inline constexpr int kRankMinWidth = 300;
inline constexpr int kColumnGap = 12;
inline constexpr int kHysteresis = 32;
inline constexpr int kDualColumnEnterWidth = 1000;
inline constexpr int kDualColumnExitWidth = kDualColumnEnterWidth - kHysteresis;

inline Mode resolveMode(int viewportWidth, Mode previousMode)
{
    if (viewportWidth >= kDualColumnEnterWidth)
        return Mode::DualColumn;
    if (viewportWidth < kDualColumnExitWidth)
        return Mode::SingleColumn;
    return previousMode;
}

} // namespace DashboardLayout
