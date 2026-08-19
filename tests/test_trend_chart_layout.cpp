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

void assertCellsFillGrid(const TrendChartLayout::HeatmapLayout &layout)
{
    for (const TrendChartLayout::HeatCell &cell : layout.cells) {
        assert(cell.rect.left() >= layout.gridRect.left() - 0.001);
        assert(cell.rect.top() >= layout.gridRect.top() - 0.001);
        assert(cell.rect.right() <= layout.gridRect.right() + 0.001);
        assert(cell.rect.bottom() <= layout.gridRect.bottom() + 0.001);
    }
    // 网格铺满整个格子区域，不留下居中留白。
    assert(nearlyEqual(layout.gridRect.width(), layout.columns * layout.cellWidth
        + qMax(0, layout.columns - 1) * layout.horizontalGap));
    assert(nearlyEqual(layout.gridRect.height(), layout.rows * layout.cellHeight
        + qMax(0, layout.rows - 1) * layout.verticalGap));
    assert(nearlyEqual(layout.cells.first().rect.left(), layout.gridRect.left()));
    assert(nearlyEqual(layout.cells.first().rect.top(), layout.gridRect.top()));
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
    const auto layout = TrendChartLayout::makeWeekHeatmapLayout(QSizeF(900, 260), monday);

    assert(!layout.isMonth);
    assert(layout.columns == 7);
    assert(layout.rows == 1);
    assert(layout.cells.size() == 7);
    assert(layout.monthInfoRect.isEmpty());
    assertCellsFillGrid(layout);
    for (int index = 0; index < layout.cells.size(); ++index) {
        assert(layout.cells[index].date == monday.addDays(index));
        assert(TrendChartLayout::heatCellAt(layout, layout.cells[index].rect.center()) == index);
    }
    std::cout << "test_week_heatmap_has_fixed_monday_to_sunday_columns PASS" << std::endl;
}

void test_month_heatmap_uses_continuous_date_matrix()
{
    const auto layout = TrendChartLayout::makeMonthHeatmapLayout(
        QSizeF(900, 300), QDate(2026, 8, 1));

    assert(layout.isMonth);
    assert(layout.columns == 8);
    assert(layout.rows == 4);
    assert(layout.cells.size() == 31);
    assert(!layout.monthInfoRect.isEmpty());
    assertCellsFillGrid(layout);
    for (int index = 0; index < layout.cells.size(); ++index) {
        assert(layout.cells[index].date == QDate(2026, 8, index + 1));
        assert(TrendChartLayout::heatCellAt(layout, layout.cells[index].rect.center()) == index);
    }
    assert(nearlyEqual(layout.cells.first().rect.left(), layout.gridRect.left()));
    assert(nearlyEqual(layout.cells[8].rect.top(), layout.gridRect.top()
        + layout.cellHeight + layout.verticalGap));
    std::cout << "test_month_heatmap_uses_continuous_date_matrix PASS" << std::endl;
}

void test_month_heatmap_uses_four_rows()
{
    const auto thirtyOneDays = TrendChartLayout::makeMonthHeatmapLayout(
        QSizeF(700, 260), QDate(2026, 8, 1));
    const auto twentyEightDays = TrendChartLayout::makeMonthHeatmapLayout(
        QSizeF(700, 260), QDate(2026, 2, 1));
    assert(TrendChartLayout::monthRowCount(QDate(2026, 8, 1)) == 4);
    assert(TrendChartLayout::monthRowCount(QDate(2026, 2, 1)) == 4);
    assert(thirtyOneDays.columns == 8);
    assert(twentyEightDays.columns == 7);
    // 矩形格各自铺满宽高，不再强制正方形。
    assert(thirtyOneDays.cellWidth > thirtyOneDays.cellHeight);
    assertCellsFillGrid(thirtyOneDays);
    assertCellsFillGrid(twentyEightDays);
    std::cout << "test_month_heatmap_uses_four_rows PASS" << std::endl;
}

void test_wide_heatmap_fills_stage_with_insight_panel()
{
    const QSizeF size(900, 300);
    const auto layout = TrendChartLayout::makeMonthHeatmapLayout(size, QDate(2026, 8, 1));

    assert(!layout.compactInsight);
    assert(nearlyEqual(layout.contentRect.left(), 12.0));
    assert(nearlyEqual(layout.contentRect.right(), size.width() - 12.0));
    assert(nearlyEqual(layout.contentRect.top(), 12.0));
    assert(nearlyEqual(layout.contentRect.bottom(), size.height() - 12.0));
    // 舞台内边距 12：矩阵与概览不贴舞台边。
    assert(nearlyEqual(layout.matrixRect.left(), layout.contentRect.left() + 12.0));
    assert(nearlyEqual(layout.matrixRect.top(), layout.contentRect.top() + 12.0));
    assert(nearlyEqual(layout.insightRect.right(), layout.contentRect.right() - 12.0));
    assert(layout.matrixRect.right() < layout.insightRect.left());
    assert(nearlyEqual(layout.matrixRect.top(), layout.insightRect.top()));
    assert(nearlyEqual(layout.matrixRect.bottom(), layout.insightRect.bottom()));
    assert(layout.legendRect.left() >= layout.insightRect.left());
    assert(layout.legendRect.right() <= layout.insightRect.right() + 0.001);
    assertCellsFillGrid(layout);
    std::cout << "test_wide_heatmap_fills_stage_with_insight_panel PASS" << std::endl;
}

void test_compact_heatmap_stacks_summary_above_matrix()
{
    const QSizeF size(520, 320);
    const auto layout = TrendChartLayout::makeMonthHeatmapLayout(size, QDate(2026, 8, 1));

    assert(layout.compactInsight);
    assert(layout.insightRect.bottom() < layout.matrixRect.top());
    assert(nearlyEqual(layout.insightRect.left(), layout.contentRect.left() + 12.0));
    assert(nearlyEqual(layout.insightRect.right(), layout.contentRect.right() - 12.0));
    assert(nearlyEqual(layout.matrixRect.left(), layout.contentRect.left() + 12.0));
    assert(nearlyEqual(layout.matrixRect.right(), layout.contentRect.right() - 12.0));
    assert(layout.legendRect.left() >= layout.insightRect.left());
    assertCellsFillGrid(layout);
    std::cout << "test_compact_heatmap_stacks_summary_above_matrix PASS" << std::endl;
}

void test_month_heatmap_stays_valid_in_small_areas()
{
    for (const QSizeF size : {QSizeF(180, 260), QSizeF(700, 120), QSizeF(0, 0)}) {
        const auto layout = TrendChartLayout::makeMonthHeatmapLayout(size, QDate(2026, 8, 1));
        assert(layout.cellWidth >= 0.0);
        assert(layout.cellHeight >= 0.0);
        assert(layout.contentRect.left() >= 0.0);
        assert(layout.contentRect.top() >= 0.0);
        assert(layout.contentRect.right() <= size.width() + 0.001);
        assert(layout.contentRect.bottom() <= size.height() + 0.001);
        assertCellsFillGrid(layout);
    }
    std::cout << "test_month_heatmap_stays_valid_in_small_areas PASS" << std::endl;
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
    test_month_heatmap_uses_continuous_date_matrix();
    test_month_heatmap_uses_four_rows();
    test_wide_heatmap_fills_stage_with_insight_panel();
    test_compact_heatmap_stacks_summary_above_matrix();
    test_month_heatmap_stays_valid_in_small_areas();
    test_readable_badge_text_has_high_contrast();
    std::cout << "All trend chart layout tests passed!" << std::endl;
    return 0;
}
