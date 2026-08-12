#include "ui/trend_card.h"
#include "ui/design_tokens.h"
#include "ui/theme_manager.h"
#include "ui/ui_utils.h"

#include <QButtonGroup>
#include <QEvent>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QToolTip>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

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
    QDate today = QDate::currentDate();
    return today.addDays(-today.dayOfWeek() + 1);
}

/// 求让方块铺满区域且尽量接近正方形的列数：遍历列数，选长宽差最小的。
static int bestColumns(int totalDays, double availW, double availH, double gap)
{
    int best = 1;
    double bestDiff = 1e18;
    for (int c = 1; c <= totalDays; ++c) {
        const int rows = (totalDays + c - 1) / c;
        const double cellW = (availW - (c - 1) * gap) / c;
        const double cellH = (availH - (rows - 1) * gap) / rows;
        if (cellW < 4.0 || cellH < 4.0) continue;
        const double diff = qAbs(cellW - cellH);
        if (diff < bestDiff) {
            bestDiff = diff;
            best = c;
        }
    }
    return best;
}

/// 热力图单个格子的命中信息，供绘制与鼠标悬停提示共用。
struct HeatCell
{
    QDate date;
    QRectF rect;
    int value = 0;
    int level = 0;      // 0（空）~ 4（最强）
    bool future = false; // 未来日期：淡化显示，不参与色阶
};

/// 热力图布局结果：格子几何 + 图例区域。
struct HeatLayout
{
    QVector<HeatCell> cells;
    double x0 = 0;
    double y0 = 0;
    double cellW = 0;   // 格子宽
    double cellH = 0;   // 格子高
    double gap = 0;     // 相邻格子间距
    double side = 0;    // 格子短边，用于圆角/字号/描边缩放
    double legendH = 0; // 底部图例预留高度
    int columns = 0;    // 实际列数（周模式 7，月模式自适应）
};

/// Chart surface that fills its widget and owns the data.
class TrendChartArea : public QWidget
{
public:
    explicit TrendChartArea(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        // 允许图表区收缩：最小窗口（980x780）下卡片高度受限，过大的 minimum
        // 会让图表底边溢出卡片边框（日标签被遮挡）。绘制由 chartH<60 兜底。
        setMinimumHeight(96);
        setMouseTracking(true); // 无按键也能收到 mouseMoveEvent 以显示悬停提示
        connect(ThemeManager::instance(), &ThemeManager::themeChanged,
                this, [this](ThemeManager::Theme) { update(); });
    }

    void setData(const QVector<QVariantMap> &weekData)
    {
        const QDate monday = currentMonday();
        m_weekData.clear();
        for (int i = 0; i < 7; ++i)
            m_weekData[monday.addDays(i).toString(Qt::ISODate)] = 0;

        for (const auto &item : weekData) {
            const QString d = item[QStringLiteral("d")].toString();
            if (m_weekData.contains(d))
                m_weekData[d] = item[QStringLiteral("total_seconds")].toInt();
        }

        if (m_period == QLatin1String("week"))
            rebuildActive();
    }

    void setMonthData(const QVector<QVariantMap> &monthData)
    {
        const QDate today = QDate::currentDate();
        m_monthData.clear();
        for (int d = 1; d <= today.daysInMonth(); ++d)
            m_monthData[QDate(today.year(), today.month(), d).toString(Qt::ISODate)] = 0;

        for (const auto &item : monthData) {
            const QString d = item[QStringLiteral("d")].toString();
            if (m_monthData.contains(d))
                m_monthData[d] = item[QStringLiteral("total_seconds")].toInt();
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
        update();
    }

    void setPeriod(const QString &period)
    {
        m_period = period;
        rebuildActive();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::TextAntialiasing);

        if (m_format == QLatin1String("heatmap")) {
            paintHeatmap(painter);
            return;
        }

        const double pw = width();
        const double ph = height();
        const double leftInset = 16;
        const double rightInset = 16;
        const double chartH = ph - 34; // room for day labels at bottom
        if (chartH < 60) return;

        bool allZero = true;
        for (auto it = m_data.begin(); it != m_data.end(); ++it) {
            if (it.value() > 0) { allZero = false; break; }
        }

        if (allZero) {
            painter.setFont(DesignTokens::appFont(13));
            painter.setPen(DesignTokens::kTextFaint());
            painter.drawText(rect(), Qt::AlignCenter,
                             QString::fromUtf8("\xe6\x9a\x82\xe6\x97\xa0\xe6\x95\xb0\xe6\x8d\xae"));
            return;
        }

        const QString todayKey = QDate::currentDate().toString(Qt::ISODate);
        const int count = 7;
        const double baselineY = chartH;

        if (m_type == QLatin1String("bar")) {
            const double step = (pw - leftInset - rightInset) / count;
            const double barW = qMin(34.0, step * 0.5);

            painter.setPen(QPen(DesignTokens::kBorder(), 1.0));
            painter.drawLine(QPointF(leftInset, baselineY),
                             QPointF(pw - rightInset, baselineY));

            for (int i = 0; i < count; ++i) {
                const double x = leftInset + i * step + (step - barW) / 2.0;
                const QString d = currentMonday().addDays(i).toString(Qt::ISODate);
                const int val = m_data.value(d, 0);
                const bool isToday = (d == todayKey);
                const double barH = (static_cast<double>(val) / m_maxVal)
                                    * qMax(0.0, chartH - 46);

                if (val > 0) {
                    QPainterPath path;
                    path.addRoundedRect(QRectF(x, baselineY - barH, barW, barH), 5, 5);
                    QLinearGradient gradient(0, baselineY - barH, 0, baselineY);
                    gradient.setColorAt(0.0, DesignTokens::kChartGradientTop());
                    gradient.setColorAt(1.0, DesignTokens::kChartGradientBottom());
                    painter.setPen(Qt::NoPen);
                    painter.setBrush(gradient);
                    painter.drawPath(path);

                    painter.setFont(DesignTokens::appFont(9));
                    painter.setPen(DesignTokens::kTextFaint());
                    painter.drawText(QRectF(x - 10, baselineY - barH - 15, barW + 20, 12),
                                     Qt::AlignCenter, UiUtils::formatCompact(val));
                }

                painter.setFont(DesignTokens::appFont(
                    isToday ? 11 : 10, isToday ? QFont::Bold : QFont::Medium));
                painter.setPen(isToday ? DesignTokens::kAccent() : DesignTokens::kTextMute());
                painter.drawText(QRectF(x - 14, baselineY + 6, barW + 28, 18),
                                 Qt::AlignCenter, kDayCn[i]);
            }
        } else {
            const double step = (pw - leftInset - rightInset) / (count - 1);
            const double usableH = qMax(0.0, chartH - 30);

            QVector<QPointF> points(count);
            for (int i = 0; i < count; ++i) {
                const double x = leftInset + i * step;
                const QString d = currentMonday().addDays(i).toString(Qt::ISODate);
                const int val = m_data.value(d, 0);
                const double y = baselineY - (static_cast<double>(val) / m_maxVal) * usableH;
                points[i] = QPointF(x, y);
            }

            QPainterPath area;
            area.moveTo(points.first());
            for (int i = 0; i < count - 1; ++i) {
                const QPointF c1((points[i].x() + points[i + 1].x()) / 2.0, points[i].y());
                const QPointF c2((points[i].x() + points[i + 1].x()) / 2.0, points[i + 1].y());
                area.cubicTo(c1, c2, points[i + 1]);
            }
            area.lineTo(points.last().x(), baselineY);
            area.lineTo(points.first().x(), baselineY);
            area.closeSubpath();

            QLinearGradient areaGrad(0, 0, 0, baselineY);
            areaGrad.setColorAt(0.0, DesignTokens::kChartAreaTop());
            areaGrad.setColorAt(1.0, DesignTokens::kChartAreaBottom());
            painter.setPen(Qt::NoPen);
            painter.setBrush(areaGrad);
            painter.drawPath(area);

            QPainterPath line;
            line.moveTo(points.first());
            for (int i = 0; i < count - 1; ++i) {
                const QPointF c1((points[i].x() + points[i + 1].x()) / 2.0, points[i].y());
                const QPointF c2((points[i].x() + points[i + 1].x()) / 2.0, points[i + 1].y());
                line.cubicTo(c1, c2, points[i + 1]);
            }
            QLinearGradient strokeGrad(points.first().x(), 0, points.last().x(), 0);
            strokeGrad.setColorAt(0.0, DesignTokens::kChartGradientTop());
            strokeGrad.setColorAt(1.0, DesignTokens::kChartGradientBottom());
            QPen linePen(QBrush(strokeGrad), 2.5);
            linePen.setCapStyle(Qt::RoundCap);
            linePen.setJoinStyle(Qt::RoundJoin);
            painter.setPen(linePen);
            painter.setBrush(Qt::NoBrush);
            painter.drawPath(line);

            for (int i = 0; i < count; ++i) {
                const QString d = currentMonday().addDays(i).toString(Qt::ISODate);
                const bool isToday = (d == todayKey);
                const int val = m_data.value(d, 0);

                if (val > 0) {
                    painter.setBrush(DesignTokens::kAccent());
                    painter.setPen(Qt::NoPen);
                    painter.drawEllipse(points[i], isToday ? 4.0 : 2.5, isToday ? 4.0 : 2.5);
                }

                painter.setFont(DesignTokens::appFont(
                    isToday ? 11 : 10, isToday ? QFont::Bold : QFont::Medium));
                painter.setPen(isToday ? DesignTokens::kAccent() : DesignTokens::kTextMute());
                painter.drawText(QRectF(points[i].x() - 14, baselineY + 6, 28, 18),
                                 Qt::AlignCenter, kDayCn[i]);
            }
        }
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        if (m_format != QLatin1String("heatmap")) {
            QToolTip::hideText();
            return;
        }
        const HeatLayout layout = heatLayout();
        const QPointF pos = event->position();
        for (const HeatCell &cell : layout.cells) {
            if (cell.rect.contains(pos)) {
                QToolTip::showText(event->globalPosition().toPoint(),
                                   heatTooltip(cell), this);
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
        QVector<int> vals;
        for (auto it = m_data.begin(); it != m_data.end(); ++it) {
            m_maxVal = qMax(m_maxVal, it.value());
            if (it.value() > 0) vals.append(it.value());
        }
        if (m_maxVal == 0) m_maxVal = 1;

        // 分位数分箱：把非零值按 25/50/75 百分位切成 4 档强度，避免被单日极端值拉偏。
        m_q1 = m_q2 = m_q3 = 0;
        std::sort(vals.begin(), vals.end());
        if (!vals.isEmpty()) {
            const auto quantile = [&vals](double p) {
                const double idx = p * (vals.size() - 1);
                const int lo = static_cast<int>(std::floor(idx));
                const int hi = static_cast<int>(std::ceil(idx));
                if (lo == hi) return vals[lo];
                const double frac = idx - lo;
                return static_cast<int>(vals[lo] + (vals[hi] - vals[lo]) * frac + 0.5);
            };
            m_q1 = quantile(0.25);
            m_q2 = quantile(0.50);
            m_q3 = quantile(0.75);
        }
        update();
    }

    /// 0（空）~ 4（最强）：基于分位数把秒数映射到离散色阶。
    int computeLevel(int val) const
    {
        if (val <= 0) return 0;
        if (m_q1 == m_q3) return 2; // 非零值全部相同 → 中档
        if (val <= m_q1) return 1;
        if (val <= m_q2) return 2;
        if (val <= m_q3) return 3;
        return 4;
    }

    /// 计算热力图布局：方块铺满整个可用区域（宽高各自均分、尽量接近正方形），
    /// 底部预留图例。周模式固定 7 列，月模式按容器宽高比动态选列数。
    HeatLayout heatLayout() const
    {
        const double leftInset = 16;
        const double rightInset = 16;
        const double legendH = 16; // 底部「少 → 多」图例
        const double bottomInset = 6;

        const double availW = width() - leftInset - rightInset;
        const double availH = height() - legendH - bottomInset;
        const double gap = 4.0;

        const QDate today = QDate::currentDate();

        // 组装日期：周模式按周一~周日，月模式按自然日 1..N。
        QVector<QDate> days;
        if (m_period == QLatin1String("month")) {
            for (int d = 1; d <= today.daysInMonth(); ++d)
                days.append(QDate(today.year(), today.month(), d));
        } else {
            const QDate monday = currentMonday();
            for (int i = 0; i < 7; ++i) days.append(monday.addDays(i));
        }

        const int columns = (m_period == QLatin1String("month"))
            ? bestColumns(days.size(), availW, availH, gap)
            : 7;
        const int rows = qMax(1, (days.size() + columns - 1) / columns);

        // 铺满：宽高分别均分，方块尽量接近正方形但不强制相等。
        const double cellW = qMax(4.0, (availW - (columns - 1) * gap) / columns);
        const double cellH = qMax(4.0, (availH - (rows - 1) * gap) / rows);

        HeatLayout layout;
        layout.cellW = cellW;
        layout.cellH = cellH;
        layout.gap = gap;
        layout.side = qMin(cellW, cellH);
        layout.legendH = legendH;
        layout.columns = columns;

        // 无居中：网格从可用区左上角铺满到右下角。
        layout.x0 = leftInset;
        layout.y0 = 0;

        for (int idx = 0; idx < days.size(); ++idx) {
            const QDate d = days[idx];
            const int col = idx % columns;
            const int row = idx / columns;
            HeatCell cell;
            cell.date = d;
            cell.rect = QRectF(layout.x0 + col * (cellW + gap),
                               layout.y0 + row * (cellH + gap),
                               cellW, cellH);
            cell.value = m_data.value(d.toString(Qt::ISODate), 0);
            cell.future = d > today;
            cell.level = cell.future ? 0 : computeLevel(cell.value);
            layout.cells.append(cell);
        }
        return layout;
    }

    void paintHeatmap(QPainter &painter)
    {
        const HeatLayout layout = heatLayout();
        const double side = layout.side;

        // 圆角/描边/字号随格子边长缩放，避免小格子视觉比例失衡。
        const double radius = qBound(1.0, side * 0.15, 5.0);
        const double borderW = qBound(1.0, side * 0.09, 2.0);
        const int dayFont = qBound(7, static_cast<int>(side * 0.42), 12);
        const QString todayKey = QDate::currentDate().toString(Qt::ISODate);

        for (const HeatCell &cell : layout.cells) {
            const bool isToday = (cell.date.toString(Qt::ISODate) == todayKey);

            QColor fill = DesignTokens::heatLevel(cell.level);
            if (cell.future)
                fill.setAlpha(45); // 未来日期淡化，与空数据区分

            painter.setPen(Qt::NoPen);
            painter.setBrush(fill);
            painter.drawRoundedRect(cell.rect, radius, radius);

            if (isToday) {
                painter.setBrush(Qt::NoBrush);
                painter.setPen(QPen(DesignTokens::kAccent(), borderW));
                painter.drawRoundedRect(cell.rect, radius, radius);
            }

            // 格子过小时省略日期数字，仅保留色块。
            if (side >= 14) {
                const bool lightFill = fill.lightness() > 150 && !cell.future;
                painter.setPen(cell.future ? DesignTokens::kTextFaint()
                              : (lightFill ? DesignTokens::kTextStrong()
                                           : QColor("#FFFFFF")));
                painter.setFont(DesignTokens::appFont(
                    dayFont, isToday ? QFont::Bold : QFont::Normal));
                painter.drawText(cell.rect, Qt::AlignCenter,
                                 QString::number(cell.date.day()));
            }
        }

        paintLegend(painter, layout);
    }

    /// 底部「少 → 多」图例：5 个色块，与 GitHub 贡献图一致。
    void paintLegend(QPainter &painter, const HeatLayout &layout)
    {
        const double block = qBound(6.0, layout.side * 0.9, 10.0);
        const double lg = 3.0; // 色块间距
        const double rowH = layout.legendH;

        QFontMetrics fm(DesignTokens::appFont(9));
        const QString less = QString::fromUtf8("\xe5\xb0\x91"); // 少
        const QString more = QString::fromUtf8("\xe5\xa4\x9a"); // 多
        const double twLess = fm.horizontalAdvance(less);
        const double twMore = fm.horizontalAdvance(more);
        const double pad = 6.0;
        const double total = twLess + pad + 5.0 * block + 4.0 * lg + pad + twMore;

        const double gridW = layout.columns * layout.cellW
                           + (layout.columns - 1) * layout.gap;
        double x = layout.x0 + gridW - total;
        const double y = height() - rowH;

        painter.setFont(DesignTokens::appFont(9));
        painter.setPen(DesignTokens::kTextMute());
        painter.drawText(QRectF(x, y, twLess, rowH),
                         Qt::AlignVCenter | Qt::AlignLeft, less);
        x += twLess + pad;
        for (int level = 0; level < 5; ++level) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(DesignTokens::heatLevel(level));
            painter.drawRoundedRect(QRectF(x, y + (rowH - block) / 2.0,
                                           block, block), 2, 2);
            x += block + lg;
        }
        x += pad - lg;
        painter.setPen(DesignTokens::kTextMute());
        painter.drawText(QRectF(x, y, twMore, rowH),
                         Qt::AlignVCenter | Qt::AlignLeft, more);
    }

    QString heatTooltip(const HeatCell &cell) const
    {
        QString when;
        if (m_period == QLatin1String("month"))
            when = QString::fromUtf8("%1月%2日")
                       .arg(cell.date.month()).arg(cell.date.day());
        else
            when = kDayCn[cell.date.dayOfWeek() - 1];

        if (cell.future)
            return QString::fromUtf8("%1 · 未到来").arg(when);
        const QString duration = cell.value > 0
            ? UiUtils::formatDuration(cell.value)
            : QString::fromUtf8("无记录");
        return QString::fromUtf8("%1 · %2").arg(when, duration);
    }

    QMap<QString, int> m_weekData;
    QMap<QString, int> m_monthData;
    QMap<QString, int> m_data;
    int m_maxVal = 1;
    int m_q1 = 0;   // 25% 分位
    int m_q2 = 0;   // 50% 分位
    int m_q3 = 0;   // 75% 分位
    QString m_type = QStringLiteral("bar");
    QString m_format = QStringLiteral("normal");
    QString m_period = QStringLiteral("week");
};

TrendCard::TrendCard(QWidget *parent)
    : CardFrame(QString::fromUtf8("\xe6\x9c\xac\xe5\x91\xa8\xe8\xb6\x8b\xe5\x8a\xbf"), parent)
{
    // 标题行：标题居左，柱状/折线（或周/月）切换按钮居右，
    // 与标题齐平，不再单独占一行，把空间留给图表/热力图。
    auto *header = new QHBoxLayout();
    header->setContentsMargins(0, 0, 0, 0);
    header->setSpacing(4);

    QLabel *title = titleLabel();
    contentLayout()->removeWidget(title);
    header->addWidget(title);
    header->addStretch();

    m_barBtn = new QPushButton(QString::fromUtf8("\xe6\x9f\xb1\xe7\x8a\xb6"), this);
    m_lineBtn = new QPushButton(QString::fromUtf8("\xe6\x8a\x98\xe7\xba\xbf"), this);
    m_weekBtn = new QPushButton(QString::fromUtf8("\xe5\x91\xa8"), this);
    m_monthBtn = new QPushButton(QString::fromUtf8("\xe6\x9c\x88"), this);
    for (QPushButton *btn : {m_barBtn, m_lineBtn, m_weekBtn, m_monthBtn}) {
        btn->setCursor(Qt::PointingHandCursor);
        btn->setCheckable(true);
        btn->setFixedHeight(26);
        btn->setStyleSheet(toggleStyle(btn));
    }
    m_weekBtn->setVisible(false);
    m_monthBtn->setVisible(false);

    m_group = new QButtonGroup(this);
    m_group->addButton(m_barBtn, 0);
    m_group->addButton(m_lineBtn, 1);
    m_group->setExclusive(true);

    connect(m_group, QOverload<int>::of(&QButtonGroup::idClicked),
            this, [this](int id) {
        const QString type = (id == 1) ? QStringLiteral("line") : QStringLiteral("bar");
        m_barBtn->setStyleSheet(toggleStyle(m_barBtn));
        m_lineBtn->setStyleSheet(toggleStyle(m_lineBtn));
        m_chartArea->setType(type);
        emit chartTypeChanged(type);
    });

    m_heatGroup = new QButtonGroup(this);
    m_heatGroup->addButton(m_weekBtn, 0);
    m_heatGroup->addButton(m_monthBtn, 1);
    m_heatGroup->setExclusive(true);

    connect(m_heatGroup, QOverload<int>::of(&QButtonGroup::idClicked),
            this, [this](int id) {
        const QString period = (id == 1) ? QStringLiteral("month") : QStringLiteral("week");
        m_weekBtn->setStyleSheet(toggleStyle(m_weekBtn));
        m_monthBtn->setStyleSheet(toggleStyle(m_monthBtn));
        m_chartArea->setPeriod(period);
        updateTitle();
        emit heatmapPeriodChanged(period);
    });

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
        m_barBtn->setStyleSheet(toggleStyle(m_barBtn));
        m_lineBtn->setStyleSheet(toggleStyle(m_lineBtn));
        m_weekBtn->setStyleSheet(toggleStyle(m_weekBtn));
        m_monthBtn->setStyleSheet(toggleStyle(m_monthBtn));
    });
}

QString TrendCard::chartType() const
{
    return m_barBtn->isChecked() ? QStringLiteral("bar") : QStringLiteral("line");
}

void TrendCard::setChartType(const QString &type)
{
    const bool line = (type == QStringLiteral("line"));
    m_lineBtn->setChecked(line);
    m_barBtn->setChecked(!line);
    m_barBtn->setStyleSheet(toggleStyle(m_barBtn));
    m_lineBtn->setStyleSheet(toggleStyle(m_lineBtn));
    m_chartArea->setType(type);
}

QString TrendCard::displayFormat() const
{
    return m_weekBtn->isVisible() ? QStringLiteral("heatmap") : QStringLiteral("normal");
}

void TrendCard::setDisplayFormat(const QString &format)
{
    const bool heatmap = (format == QStringLiteral("heatmap"));
    m_barBtn->setVisible(!heatmap);
    m_lineBtn->setVisible(!heatmap);
    m_weekBtn->setVisible(heatmap);
    m_monthBtn->setVisible(heatmap);
    m_chartArea->setFormat(format);
    updateTitle();
}

QString TrendCard::heatmapPeriod() const
{
    return m_monthBtn->isChecked() ? QStringLiteral("month") : QStringLiteral("week");
}

void TrendCard::setHeatmapPeriod(const QString &period)
{
    const bool month = (period == QStringLiteral("month"));
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

QString TrendCard::toggleStyle(QPushButton *btn) const
{
    if (btn->isChecked()) {
        return QString(
            "QPushButton { border: none; border-radius: 6px; padding: 0 12px;"
            " font-size: 12px; color: %1; background: %2; }")
            .arg(DesignTokens::kAccentLight().name(),
                 DesignTokens::kAccent().name());
    }
    return QString(
        "QPushButton { border: none; border-radius: 6px; padding: 0 12px;"
        " font-size: 12px; color: %1; background: transparent; }"
        "QPushButton:hover { background: %2; }")
        .arg(DesignTokens::kTextMute().name(),
             DesignTokens::kButtonHoverBg().name(QColor::HexArgb));
}

void TrendCard::setData(const QVector<QVariantMap> &weekData)
{
    m_chartArea->setData(weekData);
}

void TrendCard::setMonthData(const QVector<QVariantMap> &monthData)
{
    m_chartArea->setMonthData(monthData);
}
