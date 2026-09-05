#include "ui/trend_card.h"
#include "ui/design_tokens.h"
#include "ui/theme_manager.h"
#include "ui/trend_chart_layout.h"
#include "ui/ui_utils.h"

#include <QButtonGroup>
#include <QEvent>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QLinearGradient>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <QToolTip>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace {

const QStringList kDayCn = {
    QString::fromUtf8("\xe5\x91\xa8\xe4\xb8\x80"),
    QString::fromUtf8("\xe5\x91\xa8\xe4\xba\x8c"),
    QString::fromUtf8("\xe5\x91\xa8\xe4\xb8\x89"),
    QString::fromUtf8("\xe5\x91\xa8\xe5\x9b\x9b"),
    QString::fromUtf8("\xe5\x91\xa8\xe4\xba\x94"),
    QString::fromUtf8("\xe5\x91\xa8\xe5\x85\xad"),
    QString::fromUtf8("\xe5\x91\xa8\xe6\x97\xa5")
};

QDate currentMonday()
{
    const QDate today = QDate::currentDate();
    return today.addDays(-today.dayOfWeek() + 1);
}

struct HeatCellData
{
    TrendChartLayout::HeatCell geometry;
    int value = 0;
    int level = 0;
    bool future = false;
};

constexpr int kCompactChartHeight = 138;
constexpr int kHeatmapCardMinHeight = 300;

} // namespace

class TrendChartArea : public QWidget
{
public:
    explicit TrendChartArea(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setObjectName(QStringLiteral("trendChartArea"));
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        setMinimumHeight(kCompactChartHeight);
        setMouseTracking(true);
        connect(ThemeManager::instance(), &ThemeManager::themeChanged,
                this, [this](ThemeManager::Theme) { update(); });
        connect(ThemeManager::instance(), &ThemeManager::accentChanged,
                this, [this]() { update(); });
    }

    void setData(const QVector<QVariantMap> &weekData)
    {
        const QDate monday = currentMonday();
        m_weekData.clear();
        for (int index = 0; index < TrendChartLayout::kWeekdayCount; ++index)
            m_weekData[monday.addDays(index).toString(Qt::ISODate)] = 0;

        for (const QVariantMap &item : weekData) {
            const QString date = item[QStringLiteral("d")].toString();
            if (m_weekData.contains(date))
                m_weekData[date] = item[QStringLiteral("total_seconds")].toInt();
        }

        if (m_period == QLatin1String("week"))
            rebuildActive();
    }

    void setMonthData(const QVector<QVariantMap> &monthData)
    {
        const QDate today = QDate::currentDate();
        m_monthData.clear();
        for (int day = 1; day <= today.daysInMonth(); ++day) {
            const QDate date(today.year(), today.month(), day);
            m_monthData[date.toString(Qt::ISODate)] = 0;
        }

        for (const QVariantMap &item : monthData) {
            const QString date = item[QStringLiteral("d")].toString();
            if (m_monthData.contains(date))
                m_monthData[date] = item[QStringLiteral("total_seconds")].toInt();
        }

        if (m_period == QLatin1String("month"))
            rebuildActive();
    }

    void setType(const QString &type)
    {
        m_type = type;
        update();
    }

    void setFormat(const QString &format)
    {
        m_format = format;
        if (m_format != QLatin1String("heatmap"))
            m_period = QStringLiteral("week");
        if (parentWidget()) {
            parentWidget()->setMinimumHeight(m_format == QLatin1String("heatmap")
                ? kHeatmapCardMinHeight : DesignTokens::kTrendMinHeightNormal);
        }
        rebuildActive();
    }

    void setPeriod(const QString &period)
    {
        m_period = period;
        rebuildActive();
    }

    QSize sizeHint() const override
    {
        return QSize(420, m_format == QLatin1String("heatmap") ? 218 : kCompactChartHeight);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::TextAntialiasing);

        if (m_format == QLatin1String("heatmap"))
            paintHeatmap(painter);
        else
            paintNormal(painter);
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        const QPointF position = event->position();
        if (m_format == QLatin1String("heatmap")) {
            const TrendChartLayout::HeatmapLayout layout = heatmapLayout();
            const int index = TrendChartLayout::heatCellAt(layout, position);
            if (index >= 0) {
                const HeatCellData cell = heatCellData(layout.cells[index]);
                QToolTip::showText(event->globalPosition().toPoint(), heatTooltip(cell), this);
                return;
            }
        } else {
            const TrendChartLayout::NormalLayout layout = normalLayout();
            const int index = TrendChartLayout::normalSlotAt(layout, position);
            if (index >= 0) {
                const TrendChartLayout::NormalSlot &slot = layout.daySlots[index];
                const int value = m_weekData.value(slot.date.toString(Qt::ISODate), 0);
                QToolTip::showText(event->globalPosition().toPoint(),
                                   normalTooltip(slot.date, value), this);
                return;
            }
        }
        QToolTip::hideText();
    }

    void leaveEvent(QEvent *event) override
    {
        QToolTip::hideText();
        QWidget::leaveEvent(event);
    }

private:
    void rebuildActive()
    {
        m_data = (m_period == QLatin1String("month")) ? m_monthData : m_weekData;
        m_maxVal = 1;
        QVector<int> values;
        for (auto it = m_data.cbegin(); it != m_data.cend(); ++it) {
            m_maxVal = qMax(m_maxVal, it.value());
            if (it.value() > 0)
                values.append(it.value());
        }

        // 分位数分箱：非零值按 25/50/75 百分位映射到四档强度。
        m_q1 = m_q2 = m_q3 = 0;
        std::sort(values.begin(), values.end());
        if (!values.isEmpty()) {
            const auto quantile = [&values](double probability) {
                const double position = probability * (values.size() - 1);
                const int low = static_cast<int>(std::floor(position));
                const int high = static_cast<int>(std::ceil(position));
                if (low == high)
                    return values[low];
                const double fraction = position - low;
                return static_cast<int>(values[low]
                    + (values[high] - values[low]) * fraction + 0.5);
            };
            m_q1 = quantile(0.25);
            m_q2 = quantile(0.50);
            m_q3 = quantile(0.75);
        }
        update();
    }

    int computeLevel(int value) const
    {
        if (value <= 0)
            return 0;
        if (m_q1 == m_q3)
            return 2;
        if (value <= m_q1)
            return 1;
        if (value <= m_q2)
            return 2;
        if (value <= m_q3)
            return 3;
        return 4;
    }

    TrendChartLayout::NormalLayout normalLayout() const
    {
        return TrendChartLayout::makeNormalLayout(size(), currentMonday());
    }

    TrendChartLayout::HeatmapLayout heatmapLayout() const
    {
        if (m_period == QLatin1String("month")) {
            const QDate today = QDate::currentDate();
            return TrendChartLayout::makeMonthHeatmapLayout(size(), today);
        }
        return TrendChartLayout::makeWeekHeatmapLayout(size(), currentMonday());
    }

    HeatCellData heatCellData(const TrendChartLayout::HeatCell &geometry) const
    {
        HeatCellData cell;
        cell.geometry = geometry;
        if (!geometry.isCurrentMonth)
            return cell;
        cell.value = m_data.value(geometry.date.toString(Qt::ISODate), 0);
        cell.future = geometry.date > QDate::currentDate();
        cell.level = cell.future ? 0 : computeLevel(cell.value);
        return cell;
    }

    void paintNormal(QPainter &painter)
    {
        const TrendChartLayout::NormalLayout layout = normalLayout();
        if (layout.plotRect.height() < 60.0)
            return;

        const bool hasData = std::any_of(m_weekData.cbegin(), m_weekData.cend(),
                                         [](int value) { return value > 0; });
        painter.setPen(QPen(DesignTokens::kBorder(), 1.0));
        painter.drawLine(QPointF(layout.plotRect.left(), layout.baselineY),
                         QPointF(layout.plotRect.right(), layout.baselineY));

        if (m_type == QLatin1String("bar"))
            paintBars(painter, layout);
        else
            paintLine(painter, layout);

        paintNormalLabels(painter, layout);
        if (!hasData) {
            painter.setFont(DesignTokens::appFont(13));
            painter.setPen(DesignTokens::kTextPlaceholder());
            painter.drawText(layout.plotRect, Qt::AlignCenter,
                             QString::fromUtf8("暂无数据"));
        }
    }

    void paintBars(QPainter &painter, const TrendChartLayout::NormalLayout &layout)
    {
        const double step = layout.plotRect.width() / TrendChartLayout::kWeekdayCount;
        const double barWidth = qMin(34.0, step * 0.5);
        const double usableHeight = qMax(0.0, layout.plotRect.height() - 28.0);

        for (const TrendChartLayout::NormalSlot &slot : layout.daySlots) {
            const int value = m_weekData.value(slot.date.toString(Qt::ISODate), 0);
            if (value <= 0)
                continue;

            const double height = static_cast<double>(value) / m_maxVal * usableHeight;
            const QRectF barRect(slot.anchor.x() - barWidth / 2.0,
                                 layout.baselineY - height, barWidth, height);
            QPainterPath path;
            path.addRoundedRect(barRect, 5.0, 5.0);
            QLinearGradient gradient(0, barRect.top(), 0, layout.baselineY);
            gradient.setColorAt(0.0, DesignTokens::kChartGradientTop());
            gradient.setColorAt(1.0, DesignTokens::kChartGradientBottom());
            painter.setPen(Qt::NoPen);
            painter.setBrush(gradient);
            painter.drawPath(path);

            painter.setFont(DesignTokens::appFont(9));
            painter.setPen(DesignTokens::kChartValueText());
            painter.drawText(QRectF(barRect.left() - 10.0, barRect.top() - 15.0,
                                    barRect.width() + 20.0, 12.0),
                             Qt::AlignCenter, UiUtils::formatCompact(value));
        }
    }

    void paintLine(QPainter &painter, const TrendChartLayout::NormalLayout &layout)
    {
        const double usableHeight = qMax(0.0, layout.plotRect.height() - 30.0);
        QVector<QPointF> points;
        points.reserve(layout.daySlots.size());
        for (const TrendChartLayout::NormalSlot &slot : layout.daySlots) {
            const int value = m_weekData.value(slot.date.toString(Qt::ISODate), 0);
            points.append(QPointF(slot.anchor.x(), layout.baselineY
                - static_cast<double>(value) / m_maxVal * usableHeight));
        }

        // 两端延伸，避免折线在绘图区边缘被硬截断：
        // 左端按首段斜率反向外推到绘图区左缘，像从上一周的线延续进来；
        // 右端用同款平滑曲线落到坐标轴上，再沿轴延伸到绘图区右缘。
        QVector<QPointF> curve;
        curve.reserve(points.size() + 2);
        const QPointF &p0 = points.first();
        const QPointF &p1 = points.at(1);
        const double leftX = layout.plotRect.left();
        const double rightX = layout.plotRect.right();
        const double ratio = (p0.x() - leftX) / (p1.x() - p0.x());
        const double leftY = qBound(layout.plotRect.top(),
                                    p0.y() + (p0.y() - p1.y()) * ratio,
                                    layout.baselineY);
        curve.append(QPointF(leftX, leftY));
        curve += points;
        curve.append(QPointF(rightX, layout.baselineY));

        auto appendSmooth = [&curve](QPainterPath &path) {
            path.moveTo(curve.first());
            for (int index = 0; index < curve.size() - 1; ++index) {
                const QPointF control1((curve[index].x() + curve[index + 1].x()) / 2.0,
                                       curve[index].y());
                const QPointF control2((curve[index].x() + curve[index + 1].x()) / 2.0,
                                       curve[index + 1].y());
                path.cubicTo(control1, control2, curve[index + 1]);
            }
        };

        QPainterPath area;
        appendSmooth(area);
        area.lineTo(rightX, layout.baselineY);
        area.lineTo(leftX, layout.baselineY);
        area.closeSubpath();

        QLinearGradient areaGradient(0, 0, 0, layout.baselineY);
        areaGradient.setColorAt(0.0, DesignTokens::kChartAreaTop());
        areaGradient.setColorAt(1.0, DesignTokens::kChartAreaBottom());
        painter.setPen(Qt::NoPen);
        painter.setBrush(areaGradient);
        painter.drawPath(area);

        QPainterPath line;
        appendSmooth(line);
        QLinearGradient strokeGradient(leftX, 0, rightX, 0);
        strokeGradient.setColorAt(0.0, DesignTokens::kChartGradientTop());
        strokeGradient.setColorAt(1.0, DesignTokens::kChartGradientBottom());
        QPen linePen(QBrush(strokeGradient), 2.5);
        linePen.setCapStyle(Qt::RoundCap);
        linePen.setJoinStyle(Qt::RoundJoin);
        painter.setPen(linePen);
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(line);

        const QString todayKey = QDate::currentDate().toString(Qt::ISODate);
        for (int index = 0; index < points.size(); ++index) {
            const int value = m_weekData.value(layout.daySlots[index].date.toString(Qt::ISODate), 0);
            if (value <= 0)
                continue;
            const bool isToday = layout.daySlots[index].date.toString(Qt::ISODate) == todayKey;
            painter.setBrush(DesignTokens::kAccent());
            painter.setPen(Qt::NoPen);
            painter.drawEllipse(points[index], isToday ? 4.0 : 2.5, isToday ? 4.0 : 2.5);
        }
    }

    void paintNormalLabels(QPainter &painter, const TrendChartLayout::NormalLayout &layout)
    {
        const QString todayKey = QDate::currentDate().toString(Qt::ISODate);
        for (int index = 0; index < layout.daySlots.size(); ++index) {
            const TrendChartLayout::NormalSlot &slot = layout.daySlots[index];
            const bool isToday = slot.date.toString(Qt::ISODate) == todayKey;
            painter.setFont(DesignTokens::appFont(isToday ? 11 : 10,
                                                  isToday ? QFont::Bold : QFont::Medium));
            painter.setPen(isToday ? DesignTokens::kAccent() : DesignTokens::kTextMute());
            painter.drawText(QRectF(slot.hitRect.left(), layout.baselineY + 6.0,
                                    slot.hitRect.width(), 18.0),
                             Qt::AlignCenter, kDayCn[index]);
        }
    }

    void paintHeatmap(QPainter &painter)
    {
        const TrendChartLayout::HeatmapLayout layout = heatmapLayout();
        const double cellSide = qMin(layout.cellWidth, layout.cellHeight);
        const double radius = qBound(2.0, cellSide * 0.14, 10.0);
        const double borderWidth = qBound(1.0, cellSide * 0.055, 2.0);
        const QString todayKey = QDate::currentDate().toString(Qt::ISODate);
        bool hasRecordedDay = false;

        if (layout.isMonth) {
            const QDate today = QDate::currentDate();
            painter.setFont(DesignTokens::appFont(11, QFont::DemiBold));
            painter.setPen(DesignTokens::kTextStrong());
            painter.drawText(layout.monthInfoRect.adjusted(2.0, 0.0, -2.0, 0.0),
                             Qt::AlignLeft | Qt::AlignVCenter,
                             QString::fromUtf8("%1年%2月 · 每日使用节律")
                                 .arg(today.year()).arg(today.month()));
        }

        for (const TrendChartLayout::HeatCell &geometry : layout.cells) {
            const HeatCellData cell = heatCellData(geometry);
            const bool isToday = geometry.date.toString(Qt::ISODate) == todayKey;
            hasRecordedDay = hasRecordedDay || (!cell.future && cell.value > 0);
            QColor fill = DesignTokens::heatLevel(cell.level);
            QColor outline = Qt::transparent;
            if (cell.future) {
                fill = DesignTokens::kSurface();
                fill.setAlpha(DesignTokens::isDarkTheme() ? 22 : 74);
                outline = DesignTokens::kBorder();
                outline.setAlpha(DesignTokens::isDarkTheme() ? 70 : 90);
            }

            painter.setPen(outline.alpha() > 0 ? QPen(outline, 1.0) : Qt::NoPen);
            painter.setBrush(fill);
            painter.drawRoundedRect(geometry.rect, radius, radius);

            if (isToday) {
                painter.setBrush(Qt::NoBrush);
                painter.setPen(QPen(DesignTokens::kSurface(), borderWidth + 2.0));
                painter.drawRoundedRect(geometry.rect.adjusted(1.5, 1.5, -1.5, -1.5),
                                        radius - 1.0, radius - 1.0);
                painter.setPen(QPen(DesignTokens::kAccent(), borderWidth));
                painter.drawRoundedRect(geometry.rect.adjusted(2.5, 2.5, -2.5, -2.5),
                                        radius - 2.0, radius - 2.0);
            }

            const bool lightFill = fill.lightness() > 150 && !cell.future;
            painter.setPen(cell.future ? DesignTokens::kTextFaint()
                          : (lightFill ? DesignTokens::kTextStrong() : QColor("#FFFFFF")));
            painter.setFont(DesignTokens::appFont(qBound(9, static_cast<int>(cellSide * 0.25), 12),
                isToday ? QFont::Bold : QFont::Medium));
            painter.drawText(geometry.rect.adjusted(7.0, 4.0, -5.0, -4.0),
                             Qt::AlignLeft | Qt::AlignTop,
                             QString::number(geometry.date.day()));

            // 高格（周视图热柱）在日期下补星期标注，避免中段空荡。
            if (geometry.rect.height() >= 90.0) {
                painter.setFont(DesignTokens::appFont(10, QFont::Medium));
                painter.setPen(cell.future ? DesignTokens::kTextFaint()
                               : (lightFill ? DesignTokens::kTextMute()
                                  : QColor(255, 255, 255, 190)));
                painter.drawText(geometry.rect.adjusted(7.0, 26.0, -5.0, -4.0),
                                 Qt::AlignLeft | Qt::AlignTop,
                                 kDayCn[geometry.date.dayOfWeek() - 1]);
            }

            if (geometry.rect.width() >= 50.0 && geometry.rect.height() >= 40.0
                && !cell.future && cell.value > 0) {
                painter.setFont(DesignTokens::monoFont(qBound(8,
                    static_cast<int>(cellSide * 0.16), 10), QFont::DemiBold));
                painter.setPen(lightFill ? DesignTokens::kTextMute() : QColor("#FFFFFF"));
                painter.drawText(geometry.rect.adjusted(5.0, 16.0, -6.0, -5.0),
                                 Qt::AlignRight | Qt::AlignBottom,
                                 UiUtils::formatCompact(cell.value));
            }
        }

        if (layout.isMonth && !hasRecordedDay) {
            painter.setFont(DesignTokens::appFont(9));
            painter.setPen(DesignTokens::kTextPlaceholder());
            painter.drawText(layout.monthInfoRect.adjusted(0.0, 0.0, -2.0, 0.0),
                             Qt::AlignRight | Qt::AlignVCenter,
                             QString::fromUtf8("本月暂未记录使用时长"));
        }
        paintInsights(painter, layout);
    }

    void paintInsights(QPainter &painter, const TrendChartLayout::HeatmapLayout &layout)
    {
        int total = 0;
        int activeDays = 0;
        int peakValue = 0;
        QDate peakDate;
        for (auto it = m_data.cbegin(); it != m_data.cend(); ++it) {
            const QDate date = QDate::fromString(it.key(), Qt::ISODate);
            if (!date.isValid() || date > QDate::currentDate())
                continue;
            total += it.value();
            if (it.value() > 0)
                ++activeDays;
            if (it.value() > peakValue) {
                peakValue = it.value();
                peakDate = date;
            }
        }
        const int average = activeDays > 0 ? total / activeDays : 0;
        const QString totalText = UiUtils::formatDuration(total);
        const QString peakText = peakDate.isValid()
            ? QString::fromUtf8("%1日 · %2").arg(peakDate.day()).arg(UiUtils::formatCompact(peakValue))
            : QString::fromUtf8("暂无记录");

        if (layout.compactInsight) {
            const QRectF statsRect = layout.insightRect.adjusted(2.0, 0.0,
                                                                 -layout.legendRect.width() - 12.0, 0.0);
            painter.setFont(DesignTokens::appFont(9, QFont::Medium));
            painter.setPen(DesignTokens::kTextMute());
            painter.drawText(statsRect, Qt::AlignLeft | Qt::AlignVCenter,
                QString::fromUtf8("总计 %1   ·   活跃 %2 天   ·   日均 %3")
                    .arg(totalText).arg(activeDays).arg(UiUtils::formatCompact(average)));
            paintLegend(painter, layout.legendRect);
            return;
        }

        painter.setPen(QPen(DesignTokens::kBorder(), 1.0));
        painter.drawLine(layout.insightRect.topLeft(), layout.insightRect.bottomLeft());
        const QRectF panel = layout.insightRect.adjusted(18.0, 2.0, -4.0, -2.0);

        painter.setFont(DesignTokens::eyebrowFont(9));
        painter.setPen(DesignTokens::kTextFaint());
        painter.drawText(QRectF(panel.left(), panel.top(), panel.width(), 18.0),
                         Qt::AlignLeft | Qt::AlignVCenter,
                         layout.isMonth ? QString::fromUtf8("本月概览")
                                        : QString::fromUtf8("本周概览"));

        painter.setFont(DesignTokens::monoFont(18, QFont::DemiBold));
        painter.setPen(DesignTokens::kTextStrong());
        painter.drawText(QRectF(panel.left(), panel.top() + 22.0, panel.width(), 30.0),
                         Qt::AlignLeft | Qt::AlignVCenter, totalText);
        painter.setFont(DesignTokens::appFont(8));
        painter.setPen(DesignTokens::kTextFaint());
        painter.drawText(QRectF(panel.left(), panel.top() + 50.0, panel.width(), 18.0),
                         Qt::AlignLeft | Qt::AlignVCenter,
                         QString::fromUtf8("累计使用时长"));

        const double statsTop = panel.top() + 78.0;
        painter.setFont(DesignTokens::appFont(8));
        painter.setPen(DesignTokens::kTextFaint());
        painter.drawText(QRectF(panel.left(), statsTop, panel.width() * 0.48, 18.0),
                         Qt::AlignLeft | Qt::AlignVCenter, QString::fromUtf8("活跃天数"));
        painter.drawText(QRectF(panel.left() + panel.width() * 0.52, statsTop,
                                panel.width() * 0.48, 18.0),
                         Qt::AlignLeft | Qt::AlignVCenter, QString::fromUtf8("活跃日均"));
        painter.setFont(DesignTokens::monoFont(11, QFont::DemiBold));
        painter.setPen(DesignTokens::kText());
        painter.drawText(QRectF(panel.left(), statsTop + 18.0, panel.width() * 0.48, 22.0),
                         Qt::AlignLeft | Qt::AlignVCenter,
                         QString::fromUtf8("%1 天").arg(activeDays));
        painter.drawText(QRectF(panel.left() + panel.width() * 0.52, statsTop + 18.0,
                                panel.width() * 0.48, 22.0),
                         Qt::AlignLeft | Qt::AlignVCenter,
                         UiUtils::formatCompact(average));

        const double peakTop = statsTop + 52.0;
        painter.setFont(DesignTokens::appFont(8));
        painter.setPen(DesignTokens::kTextFaint());
        painter.drawText(QRectF(panel.left(), peakTop, panel.width(), 18.0),
                         Qt::AlignLeft | Qt::AlignVCenter, QString::fromUtf8("峰值日"));
        painter.setFont(DesignTokens::monoFont(10, QFont::DemiBold));
        painter.setPen(DesignTokens::kText());
        painter.drawText(QRectF(panel.left(), peakTop + 18.0, panel.width(), 22.0),
                         Qt::AlignLeft | Qt::AlignVCenter, peakText);

        paintLegend(painter, layout.legendRect.adjusted(18.0, 0.0, -16.0, 0.0));
    }

    void paintLegend(QPainter &painter, const QRectF &rect)
    {
        if (rect.isEmpty())
            return;
        constexpr double block = 8.0;
        constexpr double gap = 3.0;
        const QString low = QString::fromUtf8("少");
        const QString high = QString::fromUtf8("多");
        QFontMetrics metrics(DesignTokens::appFont(8));
        const double lowWidth = metrics.horizontalAdvance(low);
        const double highWidth = metrics.horizontalAdvance(high);
        const double totalWidth = lowWidth + highWidth + 12.0 + 5.0 * block + 4.0 * gap;
        double x = qMax(rect.left(), rect.right() - totalWidth);
        const double y = rect.center().y() - block / 2.0;

        painter.setFont(DesignTokens::appFont(8));
        painter.setPen(DesignTokens::kTextFaint());
        painter.drawText(QRectF(x, rect.top(), lowWidth, rect.height()),
                         Qt::AlignVCenter | Qt::AlignLeft, low);
        x += lowWidth + 6.0;
        for (int level = 0; level < 5; ++level) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(DesignTokens::heatLevel(level));
            painter.drawRoundedRect(QRectF(x, y, block, block), 2.0, 2.0);
            x += block + gap;
        }
        painter.setPen(DesignTokens::kTextFaint());
        painter.drawText(QRectF(x + 3.0, rect.top(), highWidth, rect.height()),
                         Qt::AlignVCenter | Qt::AlignLeft, high);
    }

    QString normalTooltip(const QDate &date, int value) const
    {
        const QString duration = value > 0 ? UiUtils::formatDuration(value)
                                           : QString::fromUtf8("无记录");
        return QString::fromUtf8("%1 · %2").arg(kDayCn[date.dayOfWeek() - 1], duration);
    }

    QString heatTooltip(const HeatCellData &cell) const
    {
        const QDate date = cell.geometry.date;
        const QString when = m_period == QLatin1String("month")
            ? QString::fromUtf8("%1年%2月%3日 · %4")
                  .arg(date.year()).arg(date.month()).arg(date.day())
                  .arg(kDayCn[date.dayOfWeek() - 1])
            : kDayCn[date.dayOfWeek() - 1];
        if (cell.future)
            return QString::fromUtf8("%1 · 未到来").arg(when);
        const QString duration = cell.value > 0 ? UiUtils::formatDuration(cell.value)
                                                : QString::fromUtf8("无记录");
        return QString::fromUtf8("%1 · %2").arg(when, duration);
    }

    QMap<QString, int> m_weekData;
    QMap<QString, int> m_monthData;
    QMap<QString, int> m_data;
    int m_maxVal = 1;
    int m_q1 = 0;
    int m_q2 = 0;
    int m_q3 = 0;
    QString m_type = QStringLiteral("bar");
    QString m_format = QStringLiteral("normal");
    QString m_period = QStringLiteral("week");
};

TrendCard::TrendCard(QWidget *parent)
    : CardFrame(QString::fromUtf8("\xe6\x9c\xac\xe5\x91\xa8\xe8\xb6\x8b\xe5\x8a\xbf"), parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    auto *header = new QHBoxLayout();
    header->setContentsMargins(0, 0, 0, 0);
    header->setSpacing(2);

    QLabel *title = titleLabel();
    contentLayout()->removeWidget(title);
    header->addWidget(title);
    header->addStretch();

    m_formatBtn = new QPushButton(this);
    m_formatBtn->setObjectName(QStringLiteral("trendFormatButton"));
    m_formatBtn->setCursor(Qt::PointingHandCursor);
    m_formatBtn->setFixedHeight(DesignTokens::kToggleButtonHeight);

    auto *formatMenu = new QMenu(m_formatBtn);
    UiUtils::applyMenuStyle(formatMenu);
    QAction *normalAction = formatMenu->addAction(QString::fromUtf8("\xe6\x99\xae\xe9\x80\x9a\xe8\xb6\x8b\xe5\x8a\xbf"));
    QAction *heatmapAction = formatMenu->addAction(QString::fromUtf8("\xe7\x83\xad\xe5\x8a\x9b\xe5\x9b\xbe"));
    connect(normalAction, &QAction::triggered, this, [this]() {
        setProperty("displayFormatOverride", false);
        setDisplayFormat(QStringLiteral("normal"));
        setProperty("displayFormatOverride", true);
    });
    connect(heatmapAction, &QAction::triggered, this, [this]() {
        setProperty("displayFormatOverride", false);
        setDisplayFormat(QStringLiteral("heatmap"));
        setProperty("displayFormatOverride", true);
    });
    m_formatBtn->setMenu(formatMenu);

    m_barBtn = new QPushButton(QString::fromUtf8("\xe6\x9f\xb1"), this);
    m_lineBtn = new QPushButton(QString::fromUtf8("\xe7\xba\xbf"), this);
    m_weekBtn = new QPushButton(QString::fromUtf8("\xe5\x91\xa8"), this);
    m_monthBtn = new QPushButton(QString::fromUtf8("\xe6\x9c\x88"), this);
    m_barBtn->setObjectName(QStringLiteral("trendBarButton"));
    m_lineBtn->setObjectName(QStringLiteral("trendLineButton"));
    m_weekBtn->setObjectName(QStringLiteral("trendWeekButton"));
    m_monthBtn->setObjectName(QStringLiteral("trendMonthButton"));
    for (QPushButton *button : {m_barBtn, m_lineBtn, m_weekBtn, m_monthBtn}) {
        button->setCursor(Qt::PointingHandCursor);
        button->setCheckable(true);
        button->setFixedHeight(DesignTokens::kToggleButtonHeight);
        button->setStyleSheet(toggleStyle(button));
    }

    m_group = new QButtonGroup(this);
    m_group->addButton(m_barBtn, 0);
    m_group->addButton(m_lineBtn, 1);
    m_group->setExclusive(true);
    connect(m_group, QOverload<int>::of(&QButtonGroup::idClicked), this, [this](int id) {
        const QString type = id == 1 ? QStringLiteral("line") : QStringLiteral("bar");
        setProperty("displayFormatOverride", false);
        setDisplayFormat(QStringLiteral("normal"));
        setProperty("displayFormatOverride", true);
        m_barBtn->setStyleSheet(toggleStyle(m_barBtn));
        m_lineBtn->setStyleSheet(toggleStyle(m_lineBtn));
        m_weekBtn->setStyleSheet(toggleStyle(m_weekBtn));
        m_monthBtn->setStyleSheet(toggleStyle(m_monthBtn));
        m_chartArea->setType(type);
        emit chartTypeChanged(type);
    });

    m_heatGroup = new QButtonGroup(this);
    m_heatGroup->addButton(m_weekBtn, 0);
    m_heatGroup->addButton(m_monthBtn, 1);
    m_heatGroup->setExclusive(true);
    connect(m_heatGroup, QOverload<int>::of(&QButtonGroup::idClicked), this, [this](int id) {
        const QString period = id == 1 ? QStringLiteral("month") : QStringLiteral("week");
        setProperty("displayFormatOverride", false);
        setDisplayFormat(QStringLiteral("heatmap"));
        setProperty("displayFormatOverride", true);
        m_weekBtn->setStyleSheet(toggleStyle(m_weekBtn));
        m_monthBtn->setStyleSheet(toggleStyle(m_monthBtn));
        m_chartArea->setPeriod(period);
        updateTitle();
        emit heatmapPeriodChanged(period);
    });

    header->addWidget(m_formatBtn);
    header->addWidget(m_barBtn);
    header->addWidget(m_lineBtn);
    header->addWidget(m_weekBtn);
    header->addWidget(m_monthBtn);
    contentLayout()->insertLayout(0, header);

    m_chartArea = new TrendChartArea(this);
    contentLayout()->addWidget(m_chartArea, 1);

    setChartType(QStringLiteral("bar"));
    setDisplayFormat(QStringLiteral("normal"));

    connect(ThemeManager::instance(), &ThemeManager::themeChanged,
            this, [this](ThemeManager::Theme) {
                const QString format = displayFormat().isEmpty()
                    ? QStringLiteral("normal") : displayFormat();
                const bool override = property("displayFormatOverride").toBool();
                setProperty("displayFormatOverride", false);
                setDisplayFormat(format);
                setProperty("displayFormatOverride", override);
            });
    connect(ThemeManager::instance(), &ThemeManager::accentChanged,
            this, [this]() {
                const QString format = displayFormat().isEmpty()
                    ? QStringLiteral("normal") : displayFormat();
                const bool override = property("displayFormatOverride").toBool();
                setProperty("displayFormatOverride", false);
                setDisplayFormat(format);
                setProperty("displayFormatOverride", override);
            });
}

QString TrendCard::chartType() const
{
    return m_barBtn->isChecked() ? QStringLiteral("bar") : QStringLiteral("line");
}

void TrendCard::setChartType(const QString &type)
{
    const bool line = type == QStringLiteral("line");
    m_lineBtn->setChecked(line);
    m_barBtn->setChecked(!line);
    m_barBtn->setStyleSheet(toggleStyle(m_barBtn));
    m_lineBtn->setStyleSheet(toggleStyle(m_lineBtn));
    m_chartArea->setType(type);
}

QString TrendCard::displayFormat() const
{
    return property("displayFormat").toString();
}

void TrendCard::setDisplayFormat(const QString &format)
{
    const bool heatmap = format == QStringLiteral("heatmap");
    if (property("displayFormatOverride").toBool()
        && property("displayFormat").isValid()) {
        return;
    }

    setProperty("displayFormat", heatmap ? QStringLiteral("heatmap")
                                          : QStringLiteral("normal"));
    if (!heatmap) {
        m_weekBtn->setChecked(true);
        m_monthBtn->setChecked(false);
    }
    m_formatBtn->setText(heatmap ? QString::fromUtf8("\xe7\x83\xad\xe5\x8a\x9b\xe5\x9b\xbe")
                                  : QString::fromUtf8("\xe8\xb6\x8b\xe5\x8a\xbf"));
    m_formatBtn->setStyleSheet(QStringLiteral(
        "QPushButton { border: 1px solid %1; border-radius: 6px; padding: 0 8px;"
        " font-size: 11px; color: %2; background: transparent; }"
        "QPushButton:hover { background: %3; color: %4; }")
        .arg(DesignTokens::kBorder().name(),
             DesignTokens::kTextMute().name(),
             DesignTokens::kButtonHoverBg().name(QColor::HexArgb),
             DesignTokens::kText().name())
        + UiUtils::focusBorderRule());
    m_barBtn->setVisible(!heatmap);
    m_lineBtn->setVisible(!heatmap);
    m_weekBtn->setVisible(heatmap);
    m_monthBtn->setVisible(heatmap);
    m_barBtn->setStyleSheet(toggleStyle(m_barBtn));
    m_lineBtn->setStyleSheet(toggleStyle(m_lineBtn));
    m_weekBtn->setStyleSheet(toggleStyle(m_weekBtn));
    m_monthBtn->setStyleSheet(toggleStyle(m_monthBtn));
    m_chartArea->setFormat(format);
    updateTitle();
}

QString TrendCard::heatmapPeriod() const
{
    return m_monthBtn->isChecked() ? QStringLiteral("month") : QStringLiteral("week");
}

void TrendCard::setHeatmapPeriod(const QString &period)
{
    const bool month = period == QStringLiteral("month");
    m_monthBtn->setChecked(month);
    m_weekBtn->setChecked(!month);
    m_weekBtn->setStyleSheet(toggleStyle(m_weekBtn));
    m_monthBtn->setStyleSheet(toggleStyle(m_monthBtn));
    m_chartArea->setPeriod(period);
    updateTitle();
}

void TrendCard::updateTitle()
{
    if (displayFormat() == QStringLiteral("heatmap")) {
        setTitle(heatmapPeriod() == QStringLiteral("month")
            ? QString::fromUtf8("\xe6\x9c\xac\xe6\x9c\x88\xe7\x83\xad\xe5\x8a\x9b\xe5\x9b\xbe")
            : QString::fromUtf8("\xe6\x9c\xac\xe5\x91\xa8\xe7\x83\xad\xe5\x8a\x9b\xe5\x9b\xbe"));
    } else {
        setTitle(QString::fromUtf8("\xe6\x9c\xac\xe5\x91\xa8\xe8\xb6\x8b\xe5\x8a\xbf"));
    }
}

QString TrendCard::toggleStyle(QPushButton *button) const
{
    const bool heatmapButton = button == m_weekBtn || button == m_monthBtn;
    const bool activeMode = heatmapButton
        ? displayFormat() == QLatin1String("heatmap")
        : displayFormat() == QLatin1String("normal");
    if (button->isChecked() && activeMode) {
        return QStringLiteral(
            "QPushButton { border: none; border-radius: 6px; padding: 0 8px;"
            " font-size: 11px; color: %1; background: %2; }")
            .arg(DesignTokens::kAccentLight().name(), DesignTokens::kAccent().name())
            + UiUtils::focusBorderRule();
    }
    return QStringLiteral(
        "QPushButton { border: none; border-radius: 6px; padding: 0 8px;"
        " font-size: 11px; color: %1; background: transparent; }"
        "QPushButton:hover { color: %3; background: %2; }")
        .arg(DesignTokens::kTextMute().name(),
             DesignTokens::kButtonHoverBg().name(QColor::HexArgb),
             DesignTokens::kText().name())
        + UiUtils::focusBorderRule();
}

void TrendCard::setData(const QVector<QVariantMap> &weekData)
{
    m_chartArea->setData(weekData);
}

void TrendCard::setMonthData(const QVector<QVariantMap> &monthData)
{
    m_chartArea->setMonthData(monthData);
}
