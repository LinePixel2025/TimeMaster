#pragma once

#include <QDate>
#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QVector>

namespace TrendChartLayout {

inline constexpr int kWeekdayCount = 7;
inline constexpr int kHeatmapMinReadableCellSide = 18;
inline constexpr int kHeatmapMaxUsefulCellSide = 32;

struct NormalSlot
{
    QDate date;
    QPointF anchor;
    QRectF hitRect;
};

struct NormalLayout
{
    QRectF plotRect;
    double baselineY = 0;
    QVector<NormalSlot> daySlots;
};

struct HeatCell
{
    QDate date;
    QRectF rect;
    bool isCurrentMonth = true;
};

struct HeatmapLayout
{
    QVector<HeatCell> cells;
    QRectF monthInfoRect;
    QRectF gridRect;
    QRectF legendRect;
    double cellWidth = 0;
    double cellHeight = 0;
    double gap = 0;
    int columns = kWeekdayCount;
    int rows = 0;
    bool isMonth = false;
};

NormalLayout makeNormalLayout(const QSizeF &size, const QDate &monday);
HeatmapLayout makeWeekHeatmapLayout(const QSizeF &size, const QDate &monday);
HeatmapLayout makeMonthHeatmapLayout(const QSizeF &size, const QDate &month);

int monthRowCount(const QDate &month);
int preferredMonthHeatmapHeight(int chartWidth, const QDate &month);

int normalSlotAt(const NormalLayout &layout, const QPointF &position);
int heatCellAt(const HeatmapLayout &layout, const QPointF &position);

} // namespace TrendChartLayout
