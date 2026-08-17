#pragma once

#include <array>

namespace PeriodDistributionLayout {

inline constexpr int kPeriodCount = 4;

inline std::array<int, kPeriodCount> aggregate(const std::array<int, 24> &hours)
{
    std::array<int, kPeriodCount> periods {};
    for (int hour = 0; hour < static_cast<int>(hours.size()); ++hour)
        periods[hour / 6] += hours[hour];
    return periods;
}

inline int totalSeconds(const std::array<int, kPeriodCount> &periods)
{
    int total = 0;
    for (const int seconds : periods)
        total += seconds;
    return total;
}

inline int peakPeriod(const std::array<int, kPeriodCount> &periods)
{
    int peak = -1;
    for (int index = 0; index < kPeriodCount; ++index) {
        if (periods[index] > 0 && (peak < 0 || periods[index] > periods[peak]))
            peak = index;
    }
    return peak;
}

inline int percent(int seconds, int total)
{
    return total > 0 ? seconds * 100 / total : 0;
}

} // namespace PeriodDistributionLayout
