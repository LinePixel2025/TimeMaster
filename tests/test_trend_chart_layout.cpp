#include <cassert>
#include <cmath>
#include <iostream>

#include <QCoreApplication>
#include <QDate>
#include <QSizeF>

#include "ui/design_tokens.h"
#include "ui/trend_chart_layout.h"

namespace {

bool nearlyEqual(double left, double right)
{
    return std::abs(left - right) < 0.001;
}

void assertSquareCellsInsideGrid(const TrendChartLayout::HeatmapLayout &layout)
{
    assert(nearlyEqual(layout.cellWidth, layout.cellHeight));
    for (const TrendChartLayout::HeatCell &cell : layout.cells) {
        assert(cell.rect.left() >= layout.gridRect.left() - 0.001);
        assert(cell.rect.top() >= layout.gridRect.top() - 0.001);
        assert(cell.rect.right() <= layout.gridRect.right() + 0.001);
        assert(cell.rect.bottom() <= layout.gridRect.bottom() + 0.001);
    }
}

void test_normal_slots_cover_the_chart()
{
    const QDate monday(2026, 8, 17);
    const auto layout = TrendChartLayout::makeNormalLayout(QSizeF(700, 180), monday);

    assert(layout.daySlots.size() == TrendChartLayout::kWeekdayCount);
    assert(layout.daySlots.first().date == monday);
    assert(layout.daySlots.last().date == monday.addDays(6));
    assert(layout.baselineY == 146.0);
    for (int index = 0; index < layout.daySlots.size(); ++index) {
        assert(TrendChartLayout::normalSlotAt(layout, layout.daySlots[index].hitRect.center()) == index);
        assert(layout.daySlots[index].hitRect.contains(layout.daySlots[index].anchor));
    }
    assert(TrendChartLayout::normalSlotAt(layout, QPointF(0, 40)) == -1);
    std::cout << "test_normal_slots_cover_the_chart PASS" << std::endl;
}

void test_week_heatmap_has_fixed_monday_to_sunday_columns()
{
    const QDate monday(2026, 8, 17);
    const auto layout = TrendChartLayout::makeWeekHeatmapLayout(QSizeF(700, 180), monday);

    assert(!layout.isMonth);
    assert(layout.columns == 7);
    assert(layout.rows == 1);
    assert(layout.cells.size() == 7);
    assert(layout.monthInfoRect.isEmpty());
    assertSquareCellsInsideGrid(layout);
    for (int index = 0; index < layout.cells.size(); ++index) {
        assert(layout.cells[index].isCurrentMonth);
        assert(layout.cells[index].date == monday.addDays(index));
        assert(TrendChartLayout::heatCellAt(layout, layout.cells[index].rect.center()) == index);
    }
    std::cout << "test_week_heatmap_has_fixed_monday_to_sunday_columns PASS" << std::endl;
}

void test_month_heatmap_aligns_first_day_and_pads_last_week()
{
    // 2026-08-01 是周六：月初应有周一至周五五个占位格。
    const auto layout = TrendChartLayout::makeMonthHeatmapLayout(QSizeF(700, 260), QDate(2026, 8, 1));

    assert(layout.isMonth);
    assert(layout.columns == 7);
    assert(layout.rows == 6);
    assert(layout.cells.size() == 42);
    assert(!layout.monthInfoRect.isEmpty());
    assert(layout.monthInfoRect.bottom() <= layout.gridRect.top());
    assert(layout.gridRect.bottom() <= layout.legendRect.top());
    assertSquareCellsInsideGrid(layout);
    for (int index = 0; index < 5; ++index) {
        assert(!layout.cells[index].isCurrentMonth);
        assert(!layout.cells[index].date.isValid());
        assert(TrendChartLayout::heatCellAt(layout, layout.cells[index].rect.center()) == -1);
    }
    assert(layout.cells[5].date == QDate(2026, 8, 1));
    assert(nearlyEqual(layout.cells[5].rect.left(), layout.gridRect.left()
        + 5 * (layout.cellWidth + layout.gap)));
    assert(layout.cells[35].date == QDate(2026, 8, 31));
    for (int index = 36; index < layout.cells.size(); ++index) {
        assert(!layout.cells[index].isCurrentMonth);
        assert(TrendChartLayout::heatCellAt(layout, layout.cells[index].rect.center()) == -1);
    }
    std::cout << "test_month_heatmap_aligns_first_day_and_pads_last_week PASS" << std::endl;
}

void test_month_heatmap_uses_square_cells_for_five_and_six_rows()
{
    const auto sixRows = TrendChartLayout::makeMonthHeatmapLayout(QSizeF(420, 240), QDate(2026, 8, 1));
    const auto fiveRows = TrendChartLayout::makeMonthHeatmapLayout(QSizeF(420, 240), QDate(2026, 6, 1));
    assert(TrendChartLayout::monthRowCount(QDate(2026, 8, 1)) == 6);
    assert(TrendChartLayout::monthRowCount(QDate(2026, 6, 1)) == 5);
    assertSquareCellsInsideGrid(sixRows);
    assertSquareCellsInsideGrid(fiveRows);
    assert(fiveRows.cellWidth >= sixRows.cellWidth);
    std::cout << "test_month_heatmap_uses_square_cells_for_five_and_six_rows PASS" << std::endl;
}

void test_month_heatmap_stays_valid_in_small_areas()
{
    for (const QSizeF size : {QSizeF(180, 260), QSizeF(700, 120), QSizeF(0, 0)}) {
        const auto layout = TrendChartLayout::makeMonthHeatmapLayout(size, QDate(2026, 8, 1));
        assert(layout.cellWidth >= 0.0);
        assert(layout.cellHeight >= 0.0);
        assert(layout.gridRect.left() >= 0.0);
        assert(layout.gridRect.top() >= 0.0);
        assert(layout.legendRect.top() >= layout.gridRect.bottom() - 0.001);
        assert(layout.legendRect.bottom() <= size.height() + 0.001);
        assertSquareCellsInsideGrid(layout);
    }
    std::cout << "test_month_heatmap_stays_valid_in_small_areas PASS" << std::endl;
}

void test_preferred_month_height_is_width_and_row_aware()
{
    const int fiveRows = TrendChartLayout::preferredMonthHeatmapHeight(420, QDate(2026, 6, 1));
    const int sixRows = TrendChartLayout::preferredMonthHeatmapHeight(420, QDate(2026, 8, 1));
    const int wider = TrendChartLayout::preferredMonthHeatmapHeight(700, QDate(2026, 8, 1));
    assert(sixRows > fiveRows);
    assert(wider >= sixRows);
    assert(wider <= TrendChartLayout::preferredMonthHeatmapHeight(1600, QDate(2026, 8, 1)));
    std::cout << "test_preferred_month_height_is_width_and_row_aware PASS" << std::endl;
}

void test_readable_badge_text_has_high_contrast()
{
    const QColor darkText("#111827");
    const QColor lightText(Qt::white);
    for (const QColor &badge : {QColor("#FFC53D"), QColor("#C0C4CC"), QColor("#E6A23C")})
        assert(DesignTokens::readableTextOn(badge) == darkText);
    assert(DesignTokens::readableTextOn(QColor("#101614")) == lightText);
    std::cout << "test_readable_badge_text_has_high_contrast PASS" << std::endl;
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    test_normal_slots_cover_the_chart();
    test_week_heatmap_has_fixed_monday_to_sunday_columns();
    test_month_heatmap_aligns_first_day_and_pads_last_week();
    test_month_heatmap_uses_square_cells_for_five_and_six_rows();
    test_month_heatmap_stays_valid_in_small_areas();
    test_preferred_month_height_is_width_and_row_aware();
    test_readable_badge_text_has_high_contrast();
    std::cout << "All trend chart layout tests passed!" << std::endl;
    return 0;
}
