#include "ui/trend_chart_layout.h"

#include <QtGlobal>

namespace {

constexpr double kLeftInset = 16.0;
constexpr double kRightInset = 16.0;
constexpr double kNormalLabelHeight = 34.0;
constexpr double kWeekLegendHeight = 16.0;
constexpr double kMonthInfoHeight = 22.0;
constexpr double kMonthLegendHeight = 18.0;
constexpr double kSectionGap = 6.0;
constexpr double kHeatmapGap = 4.0;

int rowsForMonth(const QDate &month)
{
    const QDate firstDay(month.year(), month.month(), 1);
    const int totalCells = firstDay.dayOfWeek() - 1 + firstDay.daysInMonth();
    return (totalCells + TrendChartLayout::kWeekdayCount - 1)
        / TrendChartLayout::kWeekdayCount;
}

double gapFor(const QSizeF &size, int rows)
{
    const double width = qMax(0.0, size.width() - kLeftInset - kRightInset);
    const double height = qMax(0.0, size.height());
    const double fixedHeight = kMonthInfoHeight + kMonthLegendHeight + 2.0 * kSectionGap;
    const double gridHeight = qMax(0.0, height - fixedHeight);
    const double widthGap = width / qMax(1, TrendChartLayout::kWeekdayCount - 1);
    const double heightGap = rows > 1 ? gridHeight / (rows - 1) : kHeatmapGap;
    return qMin(kHeatmapGap, qMax(0.0, qMin(widthGap, heightGap)));
}

TrendChartLayout::HeatmapLayout makeHeatmapLayout(const QSizeF &size,
                                                   const QVector<QDate> &dates,
                                                   int rows,
                                                   bool isMonth)
{
    TrendChartLayout::HeatmapLayout layout;
    layout.rows = rows;
    layout.isMonth = isMonth;

    const double width = qMax(0.0, size.width());
    const double height = qMax(0.0, size.height());
    const double inset = qMin(kLeftInset, width / 2.0);
    const double availableWidth = qMax(0.0, width - 2.0 * inset);
    const double infoHeight = isMonth ? qMin(kMonthInfoHeight, height) : 0.0;
    const double afterInfo = qMax(0.0, height - infoHeight);
    const double topGap = isMonth ? qMin(kSectionGap, afterInfo / 2.0) : 0.0;
    const double legendHeight = qMin(isMonth ? kMonthLegendHeight : kWeekLegendHeight,
                                     qMax(0.0, afterInfo - topGap));
    const double afterLegend = qMax(0.0, afterInfo - topGap - legendHeight);
    const double bottomGap = qMin(kSectionGap, afterLegend / 2.0);
    const double gridAreaHeight = qMax(0.0, afterLegend - bottomGap);

    const double widthGapLimit = availableWidth / qMax(1, TrendChartLayout::kWeekdayCount - 1);
    const double heightGapLimit = rows > 1 ? gridAreaHeight / (rows - 1) : kHeatmapGap;
    layout.gap = qMin(kHeatmapGap, qMax(0.0, qMin(widthGapLimit, heightGapLimit)));

    const double widthCapacity = qMax(0.0,
        (availableWidth - (TrendChartLayout::kWeekdayCount - 1) * layout.gap)
        / TrendChartLayout::kWeekdayCount);
    const double heightCapacity = rows > 0
        ? qMax(0.0, (gridAreaHeight - (rows - 1) * layout.gap) / rows)
        : 0.0;
    const double cellSide = qMin(widthCapacity, heightCapacity);
    layout.cellWidth = cellSide;
    layout.cellHeight = cellSide;

    const double gridWidth = TrendChartLayout::kWeekdayCount * cellSide
        + (TrendChartLayout::kWeekdayCount - 1) * layout.gap;
    const double gridHeight = rows * cellSide + qMax(0, rows - 1) * layout.gap;
    const double gridX = inset + (availableWidth - gridWidth) / 2.0;
    const double gridY = infoHeight + topGap + (gridAreaHeight - gridHeight) / 2.0;

    layout.monthInfoRect = isMonth ? QRectF(inset, 0.0, availableWidth, infoHeight) : QRectF();
    layout.gridRect = QRectF(gridX, gridY, gridWidth, gridHeight);
    layout.legendRect = QRectF(inset, layout.gridRect.bottom() + bottomGap,
                               availableWidth, legendHeight);

    for (int index = 0; index < dates.size(); ++index) {
        const int row = index / TrendChartLayout::kWeekdayCount;
        const int column = index % TrendChartLayout::kWeekdayCount;
        TrendChartLayout::HeatCell cell;
        cell.date = dates[index];
        cell.isCurrentMonth = cell.date.isValid();
        cell.rect = QRectF(gridX + column * (cellSide + layout.gap),
                           gridY + row * (cellSide + layout.gap),
                           cellSide, cellSide);
        layout.cells.append(cell);
    }
    return layout;
}

} // namespace

namespace TrendChartLayout {

NormalLayout makeNormalLayout(const QSizeF &size, const QDate &monday)
{
    NormalLayout layout;
    const double width = qMax(0.0, size.width());
    const double height = qMax(0.0, size.height());
    const double plotWidth = qMax(0.0, width - kLeftInset - kRightInset);
    const double plotHeight = qMax(0.0, height - kNormalLabelHeight);
    const double step = plotWidth / kWeekdayCount;

    layout.plotRect = QRectF(kLeftInset, 0.0, plotWidth, plotHeight);
    layout.baselineY = plotHeight;
    for (int index = 0; index < kWeekdayCount; ++index) {
        NormalSlot slot;
        slot.date = monday.addDays(index);
        slot.anchor = QPointF(kLeftInset + (index + 0.5) * step, layout.baselineY);
        slot.hitRect = QRectF(kLeftInset + index * step, 0.0, step, height);
        layout.daySlots.append(slot);
    }
    return layout;
}

HeatmapLayout makeWeekHeatmapLayout(const QSizeF &size, const QDate &monday)
{
    QVector<QDate> dates;
    dates.reserve(kWeekdayCount);
    for (int index = 0; index < kWeekdayCount; ++index)
        dates.append(monday.addDays(index));
    return makeHeatmapLayout(size, dates, 1, false);
}

HeatmapLayout makeMonthHeatmapLayout(const QSizeF &size, const QDate &month)
{
    const QDate firstDay(month.year(), month.month(), 1);
    const int daysInMonth = firstDay.daysInMonth();
    const int leadingCells = firstDay.dayOfWeek() - 1;
    const int rows = rowsForMonth(month);
    const int trailingCells = rows * kWeekdayCount - leadingCells - daysInMonth;

    QVector<QDate> dates;
    dates.reserve(rows * kWeekdayCount);
    for (int index = 0; index < leadingCells; ++index)
        dates.append(QDate());
    for (int day = 1; day <= daysInMonth; ++day)
        dates.append(QDate(month.year(), month.month(), day));
    for (int index = 0; index < trailingCells; ++index)
        dates.append(QDate());

    return makeHeatmapLayout(size, dates, rows, true);
}

int monthRowCount(const QDate &month)
{
    return rowsForMonth(month);
}

int preferredMonthHeatmapHeight(int chartWidth, const QDate &month)
{
    const int rows = rowsForMonth(month);
    const double availableWidth = qMax(0, chartWidth - qRound(kLeftInset + kRightInset));
    const double widthCapacity = qMax(0.0,
        (availableWidth - (kWeekdayCount - 1) * kHeatmapGap) / kWeekdayCount);
    const double side = qBound<double>(kHeatmapMinReadableCellSide,
                                       widthCapacity,
                                       kHeatmapMaxUsefulCellSide);
    const double gridHeight = rows * side + qMax(0, rows - 1) * kHeatmapGap;
    return qCeil(kMonthInfoHeight + kSectionGap + gridHeight + kSectionGap
                 + kMonthLegendHeight);
}

int normalSlotAt(const NormalLayout &layout, const QPointF &position)
{
    for (int index = 0; index < layout.daySlots.size(); ++index) {
        if (layout.daySlots[index].hitRect.contains(position))
            return index;
    }
    return -1;
}

int heatCellAt(const HeatmapLayout &layout, const QPointF &position)
{
    for (int index = 0; index < layout.cells.size(); ++index) {
        const HeatCell &cell = layout.cells[index];
        if (cell.isCurrentMonth && cell.rect.contains(position))
            return index;
    }
    return -1;
}

} // namespace TrendChartLayout
