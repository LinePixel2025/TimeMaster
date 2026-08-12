#include "ui/trend_card.h"
#include "ui/design_tokens.h"
#include "ui/theme_manager.h"
#include "ui/ui_utils.h"

#include <QButtonGroup>
#include <QHBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QVBoxLayout>

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

/// 在两个颜色之间线性插值，t 归一到 [0,1]。
static QColor lerpColor(const QColor &a, const QColor &b, double t)
{
    t = qBound(0.0, t, 1.0);
    return QColor(
        static_cast<int>(a.red() + (b.red() - a.red()) * t),
        static_cast<int>(a.green() + (b.green() - a.green()) * t),
        static_cast<int>(a.blue() + (b.blue() - a.blue()) * t),
        static_cast<int>(a.alpha() + (b.alpha() - a.alpha()) * t));
}

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

private:
    void rebuildActive()
    {
        m_data = (m_period == QLatin1String("month")) ? m_monthData : m_weekData;
        m_maxVal = 1;
        for (auto it = m_data.begin(); it != m_data.end(); ++it)
            m_maxVal = qMax(m_maxVal, it.value());
        if (m_maxVal == 0) m_maxVal = 1;
        update();
    }

    /// 热力图：周模式为单行 7 格，月模式为当月日历网格（周一开头，空位留白）。
    void paintHeatmap(QPainter &painter)
    {
        const double pw = width();
        const double ph = height();
        const double leftInset = 16;
        const double rightInset = 16;
        const double top = 20;   // 表头高度
        const double gap = 3.0;

        const double gridW = pw - leftInset - rightInset;
        const double colW = gridW / 7.0;

        // 表头：周一~周日
        painter.setFont(DesignTokens::appFont(10, QFont::Medium));
        painter.setPen(DesignTokens::kTextMute());
        for (int i = 0; i < 7; ++i) {
            painter.drawText(QRectF(leftInset + i * colW, 0, colW, top),
                             Qt::AlignCenter, kDayCn[i]);
        }

        // 组装网格单元（无效日期表示留白占位）
        QVector<QDate> cells;
        if (m_period == QLatin1String("month")) {
            const QDate today = QDate::currentDate();
            const QDate start(today.year(), today.month(), 1);
            const int leading = start.dayOfWeek() - 1; // 1=周一 → 0
            for (int i = 0; i < leading; ++i) cells.append(QDate());
            for (int d = 1; d <= today.daysInMonth(); ++d)
                cells.append(QDate(today.year(), today.month(), d));
        } else {
            const QDate monday = currentMonday();
            for (int i = 0; i < 7; ++i) cells.append(monday.addDays(i));
        }

        const int rows = (cells.size() + 6) / 7;
        const double availH = ph - top;
        double cellH = (availH - (rows - 1) * gap) / rows;
        cellH = qMin(cellH, colW * 0.8);
        if (cellH < 8) return;

        const double gridH = rows * cellH + (rows - 1) * gap;
        const double y0 = top + qMax(0.0, (availH - gridH) / 2.0);

        const QString todayKey = QDate::currentDate().toString(Qt::ISODate);

        for (int idx = 0; idx < cells.size(); ++idx) {
            const QDate d = cells[idx];
            if (!d.isValid()) continue;

            const int col = idx % 7;
            const int row = idx / 7;
            const QString key = d.toString(Qt::ISODate);
            const int val = m_data.value(key, 0);
            const bool isToday = (key == todayKey);

            const double x = leftInset + col * colW;
            const double y = y0 + row * (cellH + gap);
            const QRectF cell(x, y, colW - gap, cellH);

            QColor fill;
            if (val > 0) {
                const double t = static_cast<double>(val) / m_maxVal;
                fill = lerpColor(DesignTokens::kAccentLight(),
                                 DesignTokens::kAccent(), t);
            } else {
                fill = DesignTokens::kHeatEmpty();
            }

            painter.setPen(Qt::NoPen);
            painter.setBrush(fill);
            painter.drawRoundedRect(cell, 4, 4);

            if (isToday) {
                painter.setBrush(Qt::NoBrush);
                painter.setPen(QPen(DesignTokens::kAccent(), 1.5));
                painter.drawRoundedRect(cell, 4, 4);
            }

            const bool lightFill = fill.lightness() > 150;
            painter.setPen(lightFill ? DesignTokens::kTextStrong() : QColor("#FFFFFF"));
            painter.setFont(DesignTokens::appFont(
                9, isToday ? QFont::Bold : QFont::Normal));
            painter.drawText(cell, Qt::AlignCenter, QString::number(d.day()));
        }
    }

    QMap<QString, int> m_weekData;
    QMap<QString, int> m_monthData;
    QMap<QString, int> m_data;
    int m_maxVal = 1;
    QString m_type = QStringLiteral("bar");
    QString m_format = QStringLiteral("normal");
    QString m_period = QStringLiteral("week");
};

TrendCard::TrendCard(QWidget *parent)
    : CardFrame(QString::fromUtf8("\xe6\x9c\xac\xe5\x91\xa8\xe8\xb6\x8b\xe5\x8a\xbf"), parent)
{
    auto *toolbar = new QHBoxLayout();
    toolbar->setContentsMargins(0, 0, 0, 0);
    toolbar->addStretch();

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

    toolbar->addWidget(m_barBtn);
    toolbar->addWidget(m_lineBtn);
    toolbar->addWidget(m_weekBtn);
    toolbar->addWidget(m_monthBtn);
    contentLayout()->insertLayout(1, toolbar);

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
