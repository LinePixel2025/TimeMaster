#include "ui/chart_card.h"
#include "ui/design_tokens.h"
#include "ui/svg_icon.h"

#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QFontMetrics>

// ========== Internal helpers ==========

static const QStringList kDayCn = {
    QString::fromUtf8("\xe5\x91\xa8\xe4\xb8\x80"),   // 周一
    QString::fromUtf8("\xe5\x91\xa8\xe4\xba\x8c"),   // 周二
    QString::fromUtf8("\xe5\x91\xa8\xe4\xb8\x89"),   // 周三
    QString::fromUtf8("\xe5\x91\xa8\xe5\x9b\x9b"),   // 周四
    QString::fromUtf8("\xe5\x91\xa8\xe4\xba\x94"),   // 周五
    QString::fromUtf8("\xe5\x91\xa8\xe5\x85\xad"),   // 周六
    QString::fromUtf8("\xe5\x91\xa8\xe6\x97\xa5")    // 周日
};

/// Glow highlight colour for today's datum (semi-transparent accent).
static const QColor kGlowAccent(99, 102, 241, 38);

static QDate currentMonday()
{
    QDate today = QDate::currentDate();
    return today.addDays(-today.dayOfWeek() + 1);
}

static QString formatDuration(int totalSeconds)
{
    int hours = totalSeconds / 3600;
    int mins = (totalSeconds % 3600) / 60;
    return (hours > 0)
        ? QString("%1h%2").arg(hours).arg(mins, 2, 10, QChar('0'))
        : QString("%1m").arg(mins);
}

// ========== WeeklyBar ==========

WeeklyBar::WeeklyBar(QWidget *parent)
    : QWidget(parent)
{
    setMinimumHeight(120);
}

void WeeklyBar::setData(const QVector<QVariantMap> &weekData)
{
    QDate monday = currentMonday();
    m_data.clear();
    m_maxVal = 1;

    for (int i = 0; i < 7; ++i)
        m_data[monday.addDays(i).toString(Qt::ISODate)] = 0;

    for (const auto &item : weekData) {
        QString d = item["d"].toString();
        if (m_data.contains(d))
            m_data[d] = item["total_seconds"].toInt();
    }

    for (auto it = m_data.begin(); it != m_data.end(); ++it)
        m_maxVal = qMax(m_maxVal, it.value());

    if (m_maxVal == 0) m_maxVal = 1;
    update();
}

void WeeklyBar::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);

    // Check empty state — all zero
    bool allZero = true;
    for (auto it = m_data.begin(); it != m_data.end(); ++it) {
        if (it.value() > 0) { allZero = false; break; }
    }

    const double pw = width();
    const double ph = height();
    const int barCount = 7;
    const double sideMargin = qMax(12.0, qMin(28.0, pw * 0.06));
    const double barW = qMin(32.0, (pw - sideMargin * 2) / barCount);
    const double gap = (barCount > 1)
        ? ((pw - sideMargin * 2 - barW * barCount) / (barCount - 1))
        : 0;
    const double startX = sideMargin;

    const double labelY = ph - 28.0;
    const double chartTop = qMax(20.0, ph * 0.08);
    const double chartBottom = labelY - 28.0;
    const double chartAreaH = chartBottom - chartTop;
    const double baselineY = labelY - 18.0;

    // Empty state
    if (allZero) {
        painter.setFont(DesignTokens::appFont(13));
        painter.setPen(DesignTokens::kTextFaint());
        painter.drawText(QRectF(0, chartTop, pw, chartAreaH),
                         Qt::AlignCenter,
                         QString::fromUtf8("\xe6\x9a\x82\xe6\x97\xa0\xe6\x95\xb0\xe6\x8d\xae"));
        return;
    }

    QDate monday = currentMonday();
    QString todayKey = QDate::currentDate().toString(Qt::ISODate);

    // Baseline rule
    QPen rulePen(DesignTokens::kBorder(), 1.0);
    painter.setPen(rulePen);
    painter.setBrush(Qt::NoBrush);
    painter.drawLine(QPointF(sideMargin, baselineY), QPointF(pw - sideMargin, baselineY));

    QFont labelFont = DesignTokens::appFont(10);
    QFontMetrics fmLabel(labelFont);

    for (int i = 0; i < barCount; ++i) {
        double x = startX + i * (barW + gap);
        QString d = monday.addDays(i).toString(Qt::ISODate);
        int val = m_data.value(d, 0);
        bool isToday = (d == todayKey);
        double barH = (m_maxVal > 0)
            ? (static_cast<double>(val) / m_maxVal) * chartAreaH
            : 0.0;

        // Today glow
        if (isToday) {
            QPainterPath glow;
            glow.addRoundedRect(x - 4, baselineY - 2, barW + 8, 5, 2.5, 2.5);
            painter.setPen(Qt::NoPen);
            painter.setBrush(kGlowAccent);
            painter.drawPath(glow);
        }

        // Bar
        if (barH > 0) {
            QPainterPath path;
            path.addRoundedRect(x, chartBottom - barH, barW, barH, 6, 6);
            QLinearGradient gradient(x, chartBottom - barH, x, chartBottom);
            gradient.setColorAt(0.0, QColor("#A5B4FC"));
            gradient.setColorAt(1.0, DesignTokens::kAccent());
            painter.setPen(Qt::NoPen);
            painter.setBrush(QBrush(gradient));
            painter.drawPath(path);
        }

        // Value label above bar
        if (val > 0) {
            QString label = formatDuration(val);
            painter.setFont(labelFont);
            painter.setPen(DesignTokens::kTextMute());
            double textY = (barH > 0)
                ? (chartBottom - barH - 8 - fmLabel.height())
                : (chartTop - 2);
            double labelW = fmLabel.horizontalAdvance(label) + 8.0;
            painter.drawText(QRectF(x - 4, textY, labelW, fmLabel.height() + 2),
                             Qt::AlignCenter, label);
        }

        // Day label
        QFont dayFont = isToday
            ? DesignTokens::appFont(11, QFont::Bold)
            : DesignTokens::appFont(11, QFont::Medium);
        painter.setFont(dayFont);
        painter.setPen(isToday ? DesignTokens::kAccent() : DesignTokens::kTextMute());
        QFontMetrics dayFm(dayFont);
        double dayTextW = dayFm.horizontalAdvance(kDayCn[i]) + 8.0;
        painter.drawText(QRectF(x - dayTextW / 2.0 + barW / 2.0, labelY, dayTextW, 20),
                         Qt::AlignCenter, kDayCn[i]);
    }
}

// ========== WeeklyLine ==========

WeeklyLine::WeeklyLine(QWidget *parent)
    : QWidget(parent)
{
    setMinimumHeight(120);
}

void WeeklyLine::setData(const QVector<QVariantMap> &weekData)
{
    QDate monday = currentMonday();
    m_data.clear();
    m_maxVal = 1;

    for (int i = 0; i < 7; ++i)
        m_data[monday.addDays(i).toString(Qt::ISODate)] = 0;

    for (const auto &item : weekData) {
        QString d = item["d"].toString();
        if (m_data.contains(d))
            m_data[d] = item["total_seconds"].toInt();
    }

    for (auto it = m_data.begin(); it != m_data.end(); ++it)
        m_maxVal = qMax(m_maxVal, it.value());

    if (m_maxVal == 0) m_maxVal = 1;
    update();
}

void WeeklyLine::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);

    const double pw = width();
    const double ph = height();
    const int pointCount = 7;
    const double margin = qMax(16.0, qMin(32.0, pw * 0.06));
    const double chartW = pw - margin * 2;
    const double stepX = chartW / (pointCount - 1);
    const double labelY = ph - 28.0;
    const double chartTop = qMax(20.0, ph * 0.08);
    const double chartBottom = labelY - 28.0;
    const double chartAreaH = chartBottom - chartTop;

    // Check empty state
    bool allZero = true;
    for (auto it = m_data.begin(); it != m_data.end(); ++it) {
        if (it.value() > 0) { allZero = false; break; }
    }

    if (allZero) {
        painter.setFont(DesignTokens::appFont(13));
        painter.setPen(DesignTokens::kTextFaint());
        painter.drawText(QRectF(0, chartTop, pw, chartAreaH),
                         Qt::AlignCenter,
                         QString::fromUtf8("\xe6\x9a\x82\xe6\x97\xa0\xe6\x95\xb0\xe6\x8d\xae"));
        return;
    }

    QDate monday = currentMonday();
    QString todayKey = QDate::currentDate().toString(Qt::ISODate);

    // Compute points
    QVector<QPointF> points(pointCount);
    QVector<int> vals(pointCount);
    for (int i = 0; i < pointCount; ++i) {
        double x = margin + i * stepX;
        QString d = monday.addDays(i).toString(Qt::ISODate);
        int val = m_data.value(d, 0);
        vals[i] = val;
        double barH = (static_cast<double>(val) / m_maxVal) * chartAreaH;
        double y = chartBottom - barH;
        points[i] = QPointF(x, y);
    }

    // Area fill (cubic bezier → baseline → close)
    QPainterPath areaPath;
    areaPath.moveTo(points[0]);
    for (int i = 0; i < pointCount - 1; ++i) {
        QPointF cp1((points[i].x() + points[i + 1].x()) / 2.0, points[i].y());
        QPointF cp2((points[i].x() + points[i + 1].x()) / 2.0, points[i + 1].y());
        areaPath.cubicTo(cp1, cp2, points[i + 1]);
    }
    areaPath.lineTo(points.last().x(), chartBottom);
    areaPath.lineTo(points.first().x(), chartBottom);
    areaPath.closeSubpath();

    QLinearGradient areaGrad(0, chartTop, 0, chartBottom);
    areaGrad.setColorAt(0.0, QColor(165, 180, 252, 60));
    areaGrad.setColorAt(1.0, QColor(165, 180, 252, 0));
    painter.setPen(Qt::NoPen);
    painter.setBrush(areaGrad);
    painter.drawPath(areaPath);

    // Line stroke (cubic bezier)
    QPainterPath linePath;
    linePath.moveTo(points[0]);
    for (int i = 0; i < pointCount - 1; ++i) {
        QPointF cp1((points[i].x() + points[i + 1].x()) / 2.0, points[i].y());
        QPointF cp2((points[i].x() + points[i + 1].x()) / 2.0, points[i + 1].y());
        linePath.cubicTo(cp1, cp2, points[i + 1]);
    }

    QLinearGradient strokeGrad(points.first().x(), 0, points.last().x(), 0);
    strokeGrad.setColorAt(0.0, QColor("#A5B4FC"));
    strokeGrad.setColorAt(1.0, DesignTokens::kAccent());
    QPen linePen(QBrush(strokeGrad), 2.5);
    linePen.setCapStyle(Qt::RoundCap);
    linePen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(linePen);
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(linePath);

    // Points & labels
    QFont dotFont = DesignTokens::appFont(10);
    QFontMetrics dotFm(dotFont);

    for (int i = 0; i < pointCount; ++i) {
        QString d = monday.addDays(i).toString(Qt::ISODate);
        bool isToday = (d == todayKey);

        if (isToday) {
            // Today glow
            QPainterPath glow;
            glow.addRoundedRect(points[i].x() - 10, chartBottom - 4, 20, 5, 2.5, 2.5);
            painter.setPen(Qt::NoPen);
            painter.setBrush(kGlowAccent);
            painter.drawPath(glow);

            // Larger today dot with white center
            painter.setBrush(DesignTokens::kAccent());
            painter.setPen(Qt::NoPen);
            painter.drawEllipse(points[i], 4.0, 4.0);
            painter.setBrush(QColor("#FFFFFF"));
            painter.drawEllipse(points[i], 1.5, 1.5);
        } else if (vals[i] > 0) {
            painter.setBrush(DesignTokens::kAccent());
            painter.setPen(Qt::NoPen);
            painter.drawEllipse(points[i], 2.5, 2.5);
        }

        // Value label above point
        if (vals[i] > 0) {
            QString label = formatDuration(vals[i]);
            painter.setFont(dotFont);
            painter.setPen(DesignTokens::kTextFaint());
            double labelW = qMax(44.0, static_cast<double>(dotFm.horizontalAdvance(label)) + 8.0);
            painter.drawText(QRectF(points[i].x() - labelW / 2.0, points[i].y() - 18, labelW, 14),
                             Qt::AlignCenter, label);
        }
    }

    // Day labels
    QFont dayFont = DesignTokens::appFont(11, QFont::Medium);
    for (int i = 0; i < pointCount; ++i) {
        QString d = monday.addDays(i).toString(Qt::ISODate);
        bool isToday = (d == todayKey);
        double x = margin + i * stepX;
        painter.setFont(isToday ? DesignTokens::appFont(11, QFont::Bold) : dayFont);
        painter.setPen(isToday ? DesignTokens::kAccent() : DesignTokens::kTextMute());
        painter.drawText(QRectF(x - 16, labelY, 32, 20),
                         Qt::AlignCenter, kDayCn[i]);
    }
}

// ========== ChartCard ==========

ChartCard::ChartCard(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

void ChartCard::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(8);

    // Title row
    QHBoxLayout *titleRow = new QHBoxLayout();
    titleRow->setContentsMargins(0, 0, 0, 0);
    titleRow->setSpacing(6);

    QLabel *trendTitle = new QLabel(
        QString::fromUtf8("\xe6\xaf\x8f\xe6\x97\xa5\xe8\xb6\x8b\xe5\x8a\xbf"), this);
    trendTitle->setFont(DesignTokens::appFont(12));
    trendTitle->setStyleSheet("color: #6B7280;");
    titleRow->addWidget(trendTitle);
    titleRow->addStretch();

    // Bar toggle
    m_barBtn = new QPushButton(this);
    m_barBtn->setFixedSize(28, 28);
    m_barBtn->setCheckable(true);
    m_barBtn->setFlat(true);
    m_barBtn->setCursor(Qt::PointingHandCursor);
    m_barBtn->setIcon(makeSvgIcon(":/icons/bar-chart.svg"));
    m_barBtn->setIconSize(QSize(18, 18));
    m_barBtn->setToolTip(QString::fromUtf8("\xe6\x9f\xb1\xe7\x8a\xb6\xe5\x9b\xbe"));

    // Line toggle
    m_lineBtn = new QPushButton(this);
    m_lineBtn->setFixedSize(28, 28);
    m_lineBtn->setCheckable(true);
    m_lineBtn->setFlat(true);
    m_lineBtn->setCursor(Qt::PointingHandCursor);
    m_lineBtn->setIcon(makeSvgIcon(":/icons/trending-up.svg"));
    m_lineBtn->setIconSize(QSize(18, 18));
    m_lineBtn->setToolTip(QString::fromUtf8("\xe6\x8a\x98\xe7\xba\xbf\xe5\x9b\xbe"));

    m_chartGroup = new QButtonGroup(this);
    m_chartGroup->addButton(m_barBtn, 0);
    m_chartGroup->addButton(m_lineBtn, 1);
    m_chartGroup->setExclusive(true);

    titleRow->addWidget(m_barBtn);
    titleRow->addWidget(m_lineBtn);
    mainLayout->addLayout(titleRow);

    // Chart stack
    m_stack = new QStackedWidget(this);
    m_bar = new WeeklyBar(m_stack);
    m_line = new WeeklyLine(m_stack);
    m_stack->addWidget(m_bar);
    m_stack->addWidget(m_line);
    mainLayout->addWidget(m_stack, 1);

    // Default: bar selected
    m_barBtn->setChecked(true);
    m_stack->setCurrentIndex(0);
    applyToggleStyle(m_barBtn, true);
    applyToggleStyle(m_lineBtn, false);

    // Connect toggle
    connect(m_chartGroup, QOverload<int>::of(&QButtonGroup::idClicked),
        this, [this](int id) {
            m_stack->setCurrentIndex(id);
            applyToggleStyle(m_barBtn, m_barBtn->isChecked());
            applyToggleStyle(m_lineBtn, m_lineBtn->isChecked());
            emit chartTypeChanged(id == 1 ? QStringLiteral("line") : QStringLiteral("bar"));
        });
}

void ChartCard::setData(const QVector<QVariantMap> &weekData)
{
    m_bar->setData(weekData);
    m_line->setData(weekData);
}

void ChartCard::setChartType(const QString &type)
{
    bool isLine = (type == QStringLiteral("line"));
    if (isLine) {
        m_lineBtn->setChecked(true);
        m_barBtn->setChecked(false);
        m_stack->setCurrentIndex(1);
    } else {
        m_barBtn->setChecked(true);
        m_lineBtn->setChecked(false);
        m_stack->setCurrentIndex(0);
    }
    applyToggleStyle(m_barBtn, m_barBtn->isChecked());
    applyToggleStyle(m_lineBtn, m_lineBtn->isChecked());
}

void ChartCard::applyToggleStyle(QPushButton *btn, bool checked)
{
    if (checked) {
        btn->setStyleSheet(
            "QPushButton { background-color: rgba(99,102,241,0.1); "
            "border-radius: 6px; border: none; }");
    } else {
        btn->setStyleSheet(
            "QPushButton { background-color: transparent; "
            "border-radius: 6px; border: none; }"
            "QPushButton:hover { background-color: rgba(0,0,0,0.04); }");
    }
}
