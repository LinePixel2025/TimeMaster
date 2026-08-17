#include <cassert>
#include <iostream>

#include <QCoreApplication>

#include "ui/dashboard_layout.h"

void test_dual_column_when_viewport_reaches_desktop_threshold()
{
    using DashboardLayout::Mode;
    assert(DashboardLayout::resolveMode(1000, Mode::SingleColumn) == Mode::DualColumn);
    assert(DashboardLayout::resolveMode(1100, Mode::SingleColumn) == Mode::DualColumn);
    assert(DashboardLayout::resolveMode(1440, Mode::SingleColumn) == Mode::DualColumn);
    std::cout << "test_dual_column_when_viewport_reaches_desktop_threshold PASS" << std::endl;
}

void test_single_column_below_desktop_threshold()
{
    using DashboardLayout::Mode;
    assert(DashboardLayout::resolveMode(900, Mode::DualColumn) == Mode::SingleColumn);
    assert(DashboardLayout::resolveMode(967, Mode::SingleColumn) == Mode::SingleColumn);
    std::cout << "test_single_column_below_desktop_threshold PASS" << std::endl;
}

void test_hysteresis_keeps_current_layout_near_boundary()
{
    using DashboardLayout::Mode;
    const int betweenThresholds = DashboardLayout::kDualColumnExitWidth + 8;
    assert(betweenThresholds < DashboardLayout::kDualColumnEnterWidth);
    assert(DashboardLayout::resolveMode(betweenThresholds, Mode::DualColumn) == Mode::DualColumn);
    assert(DashboardLayout::resolveMode(betweenThresholds, Mode::SingleColumn) == Mode::SingleColumn);
    std::cout << "test_hysteresis_keeps_current_layout_near_boundary PASS" << std::endl;
}

void test_dual_column_threshold_provides_readable_columns()
{
    const int available = DashboardLayout::kDualColumnEnterWidth - DashboardLayout::kColumnGap;
    const int trendWidth = available * 3 / 5;
    const int rankWidth = available - trendWidth;
    assert(trendWidth >= DashboardLayout::kTrendMinWidth);
    assert(rankWidth >= DashboardLayout::kRankMinWidth);
    std::cout << "test_dual_column_threshold_provides_readable_columns PASS" << std::endl;
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    test_dual_column_when_viewport_reaches_desktop_threshold();
    test_single_column_below_desktop_threshold();
    test_hysteresis_keeps_current_layout_near_boundary();
    test_dual_column_threshold_provides_readable_columns();
    std::cout << "All dashboard layout tests passed!" << std::endl;
    return 0;
}
