#include "ui/trend_chart_layout.h"

#include <QtGlobal>

namespace {

constexpr double kOuterInset = 4.0;
constexpr double kStageInset = 12.0;
constexpr double kStagePadding = 12.0;
constexpr double kNormalLabelHeight = 34.0;
constexpr double kSectionGap = 8.0;
constexpr double kCellGap = 6.0;
constexpr double kInsightGap = 16.0;
constexpr double kInsightMinWidth = 140.0;
constexpr double kWideLayoutBreakpoint = 640.0;
constexpr double kCompactInsightHeight = 24.0;
constexpr double kLegendHeight = 20.0;
constexpr double kMonthInfoHeight = 22.0;

constexpr int kMonthHeatmapRows = 4;

int columnsForMonth(const QDate &month)
{
    return (month.daysInMonth() + kMonthHeatmapRows - 1) / kMonthHeatmapRows;
}

int rowsForMonth(const QDate &month)
{
    const int columns = columnsForMonth(month);
    return (month.daysInMonth() + columns - 1) / columns;
}

TrendChartLayout::HeatmapLayout makeHeatmapLayout(const QSizeF &size,
                                                   const QVector<QDate> &dates,
                                                   int columns,
                                                   int rows,
                                                   bool isMonth)
{
    TrendChartLayout::HeatmapLayout layout;
    layout.columns = columns;
    layout.rows = rows;
    layout.isMonth = isMonth;

    const double width = qMax(0.0, size.width());
    const double height = qMax(0.0, size.height());
    const double insetX = qMin(kStageInset, width / 2.0);
    const double insetY = qMin(kStageInset, height / 2.0);
    layout.contentRect = QRectF(insetX, insetY,
                                qMax(0.0, width - 2.0 * insetX),
                                qMax(0.0, height - 2.0 * insetY));

    const bool wide = layout.contentRect.width() >= kWideLayoutBreakpoint
        && layout.contentRect.height() >= 160.0;
    layout.compactInsight = !wide;

    // 舞台内边距：格子不贴舞台边，留出呼吸感。
    const double padX = qMin(kStagePadding, layout.contentRect.width() / 2.0);
    const double padY = qMin(kStagePadding, layout.contentRect.height() / 2.0);
    const QRectF inner = layout.contentRect.adjusted(padX, padY, -padX, -padY);

    if (wide) {
        const double matrixWidth = qMax(0.0,
            qMin(inner.width() - kInsightGap - kInsightMinWidth,
                 inner.width() * 0.70));
        layout.matrixRect = QRectF(inner.left(), inner.top(),
                                  matrixWidth, inner.height());
        layout.insightRect = QRectF(layout.matrixRect.right() + kInsightGap,
                                   inner.top(),
                                   qMax(0.0, inner.right()
                                       - layout.matrixRect.right() - kInsightGap),
                                   inner.height());
        const double legendTop = qMax(layout.insightRect.top() + 130.0,
                                      layout.insightRect.bottom() - kLegendHeight);
        layout.legendRect = QRectF(layout.insightRect.left(),
                                   legendTop,
                                   layout.insightRect.width(),
                                   qMin(kLegendHeight, layout.insightRect.bottom() - legendTop));
    } else {
        const double insightHeight = qMin(kCompactInsightHeight, inner.height());
        layout.insightRect = QRectF(inner.left(), inner.top(),
                                   inner.width(), insightHeight);
        const double legendWidth = qMin(136.0, layout.insightRect.width() * 0.40);
        layout.legendRect = QRectF(layout.insightRect.right() - legendWidth,
                                   layout.insightRect.top(), legendWidth,
                                   layout.insightRect.height());

        const double compactGap = qMin(kSectionGap,
            qMax(0.0, inner.height() - insightHeight));
        layout.matrixRect = QRectF(inner.left(),
                                  inner.top() + insightHeight + compactGap,
                                  inner.width(),
                                  qMax(0.0, inner.height() - insightHeight - compactGap));
    }

    QRectF gridArea = layout.matrixRect;
    if (isMonth) {
        const double infoHeight = qMin(kMonthInfoHeight, gridArea.height());
        layout.monthInfoRect = QRectF(gridArea.left(), gridArea.top(),
                                      gridArea.width(), infoHeight);
        const double infoGap = qMin(kSectionGap,
                                    qMax(0.0, gridArea.height() - infoHeight));
        gridArea = QRectF(gridArea.left(), gridArea.top() + infoHeight + infoGap,
                          gridArea.width(),
                          qMax(0.0, gridArea.height() - infoHeight - infoGap));
    }

    const double horizontalGap = qMin(kCellGap,
        gridArea.width() / qMax(1, columns - 1));
    const double verticalGap = qMin(kCellGap,
        gridArea.height() / qMax(1, rows - 1));
    const double cellWidth = columns > 0
        ? qMax(0.0, (gridArea.width() - (columns - 1) * horizontalGap) / columns)
        : 0.0;
    const double cellHeight = rows > 0
        ? qMax(0.0, (gridArea.height() - (rows - 1) * verticalGap) / rows)
        : 0.0;
    layout.cellWidth = cellWidth;
    layout.cellHeight = cellHeight;
    layout.horizontalGap = horizontalGap;
    layout.verticalGap = verticalGap;
    layout.gap = qMin(horizontalGap, verticalGap);
    layout.gridRect = gridArea;

    for (int index = 0; index < dates.size(); ++index) {
        const int row = index / columns;
        const int column = index % columns;
        TrendChartLayout::HeatCell cell;
        cell.date = dates[index];
        cell.isCurrentMonth = cell.date.isValid();
        cell.rect = QRectF(gridArea.left() + column * (cellWidth + horizontalGap),
                           gridArea.top() + row * (cellHeight + verticalGap),
                           cellWidth, cellHeight);
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
    const double plotWidth = qMax(0.0, width - 2.0 * kOuterInset);
    const double plotHeight = qMax(0.0, height - kNormalLabelHeight);
    const double step = plotWidth / kWeekdayCount;

    layout.plotRect = QRectF(kOuterInset, 0.0, plotWidth, plotHeight);
    layout.baselineY = plotHeight;
    for (int index = 0; index < kWeekdayCount; ++index) {
        NormalSlot slot;
        slot.date = monday.addDays(index);
        slot.anchor = QPointF(kOuterInset + (index + 0.5) * step, layout.baselineY);
        slot.hitRect = QRectF(kOuterInset + index * step, 0.0, step, height);
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
    return makeHeatmapLayout(size, dates, kWeekdayCount, 1, false);
}

HeatmapLayout makeMonthHeatmapLayout(const QSizeF &size, const QDate &month)
{
    const QDate firstDay(month.year(), month.month(), 1);
    const int daysInMonth = firstDay.daysInMonth();
    const int columns = columnsForMonth(month);
    const int rows = rowsForMonth(month);

    QVector<QDate> dates;
    dates.reserve(daysInMonth);
    for (int day = 1; day <= daysInMonth; ++day)
        dates.append(QDate(month.year(), month.month(), day));

    return makeHeatmapLayout(size, dates, columns, rows, true);
}

int monthRowCount(const QDate &month)
{
    return rowsForMonth(month);
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
