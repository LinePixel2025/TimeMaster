#include <array>
#include <cassert>
#include <iostream>

#include <QCoreApplication>

#include "ui/period_distribution_layout.h"

void test_hours_aggregate_into_four_periods()
{
    std::array<int, 24> hours {};
    hours[0] = 60;
    hours[5] = 120;
    hours[6] = 180;
    hours[11] = 240;
    hours[12] = 300;
    hours[17] = 360;
    hours[18] = 420;
    hours[23] = 480;

    const auto periods = PeriodDistributionLayout::aggregate(hours);
    assert(periods[0] == 180);
    assert(periods[1] == 420);
    assert(periods[2] == 660);
    assert(periods[3] == 900);
    assert(PeriodDistributionLayout::totalSeconds(periods) == 2160);
    std::cout << "test_hours_aggregate_into_four_periods PASS" << std::endl;
}

void test_peak_and_percent_handle_empty_and_ties()
{
    std::array<int, PeriodDistributionLayout::kPeriodCount> empty {};
    assert(PeriodDistributionLayout::peakPeriod(empty) == -1);
    assert(PeriodDistributionLayout::percent(60, 0) == 0);

    const std::array<int, PeriodDistributionLayout::kPeriodCount> periods = {7200, 1200, 7200, 0};
    assert(PeriodDistributionLayout::peakPeriod(periods) == 0);
    assert(PeriodDistributionLayout::percent(1800, 7200) == 25);
    std::cout << "test_peak_and_percent_handle_empty_and_ties PASS" << std::endl;
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    test_hours_aggregate_into_four_periods();
    test_peak_and_percent_handle_empty_and_ties();
    std::cout << "All period distribution layout tests passed!" << std::endl;
    return 0;
}
