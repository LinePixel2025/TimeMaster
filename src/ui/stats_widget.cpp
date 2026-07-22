#include "stats_widget.h"
#include "database/database_manager.h"
#include "icon/app_icon_provider.h"
#include "ui/design_tokens.h"
#include "ui/theme_manager.h"

#include <QPainter>
#include <QPainterPath>
#include <QConicalGradient>
#include <QLinearGradient>
#include <QRadialGradient>
#include <QFont>
#include <QDate>
#include <QtMath>
#include <QStackedWidget>
#include <QPushButton>
#include <QButtonGroup>

static QFont appFont(int size, QFont::Weight weight = QFont::Normal)
{
    QFont font("Microsoft YaHei", size, weight);
    font.setStyleStrategy(QFont::PreferAntialias);
    return font;
}

// ========== BigNumberWidget ==========

BigNumberWidget::BigNumberWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumHeight(150);
    setMinimumWidth(160);
}

void BigNumberWidget::setValue(int totalSeconds)
{
    m_value = totalSeconds;
    int totalMinutes = totalSeconds / 60;
    m_hours = totalMinutes / 60;
    m_minutes = totalMinutes % 60;
    update();
}

void BigNumberWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    double w = width();

    QString timeStr;
    if (m_hours > 0)
        timeStr = QString("%1h %2m").arg(m_hours).arg(m_minutes);
    else
        timeStr = QString("%1m").arg(m_minutes);

    QFont bigFont = appFont(28, QFont::Bold);
    QFontMetrics fmBig(bigFont);
    painter.setFont(bigFont);
    painter.setPen(DesignTokens::kTextStrong());
    painter.drawText(QRectF(0, 2, w, fmBig.height()), Qt::AlignCenter, timeStr);

    double curY = fmBig.height() + 10;

    painter.setFont(appFont(12));
    painter.setPen(DesignTokens::kTextFaint());
    QFontMetrics fmSub(appFont(12));
    painter.drawText(QRectF(0, curY, w, fmSub.height()), Qt::AlignCenter,
                     QString::fromUtf8("\xe4\xbb\x8a\xe6\x97\xa5\xe4\xb8\x93\xe6\xb3\xa8"));
    curY += fmSub.height() + 12;

    double barH = 6.0;
    double barMaxW = qMin(200.0, w - 24);
    double barX = (w - barMaxW) / 2.0;
    double ratio = qMin(static_cast<double>(m_value) / m_maxValue, 1.0);

    QPainterPath bgPath;
    bgPath.addRoundedRect(barX, curY, barMaxW, barH, barH / 2.0, barH / 2.0);
    painter.setPen(Qt::NoPen);
    painter.setBrush(DesignTokens::kProgressBg());
    painter.drawPath(bgPath);

    if (ratio > 0) {
        double filledW = barMaxW * ratio;
        QPainterPath fgPath;
        fgPath.addRoundedRect(barX, curY, filledW, barH, barH / 2.0, barH / 2.0);
        painter.setBrush(DesignTokens::kAccent());
        painter.drawPath(fgPath);
    }

    curY += barH + 10;

    if (m_value > 0) {
        painter.setFont(appFont(11));
        painter.setPen(DesignTokens::kTextFaint());
        int pct = static_cast<int>(ratio * 100 + 0.5);
        QString pctStr = (pct >= 100)
            ? QString::fromUtf8("\xe5\xb7\xb2\xe5\xae\x8c\xe6\x88\x90 >100%")
            : QString::fromUtf8("\xe5\xb7\xb2\xe5\xae\x8c\xe6\x88\x90 %1%").arg(pct);
        QFontMetrics fmPct(appFont(11));
        painter.drawText(QRectF(0, curY, w, fmPct.height()), Qt::AlignCenter, pctStr);
    } else {
        painter.setFont(appFont(11));
        painter.setPen(DesignTokens::kTextFaint());
        QFontMetrics fmPct(appFont(11));
        painter.drawText(QRectF(0, curY, w, fmPct.height()), Qt::AlignCenter,
                         QString::fromUtf8("\xe6\x9a\x82\xe6\x97\xa0\xe8\xae\xb0\xe5\xbd\x95"));
    }
}

// ========== GlassCard ==========

GlassCard::GlassCard(QWidget *parent)
    : QFrame(parent)
{
    setStyleSheet("GlassCard { background-color: rgba(255, 255, 255, 0); border-radius: 16px; }");
}

void GlassCard::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QPainterPath path;
    path.addRoundedRect(0, 0, width(), height(), 16, 16);
    QLinearGradient gradient(0, 0, 0, height());
    gradient.setColorAt(0.0, QColor(255, 255, 255, 190));
    gradient.setColorAt(1.0, QColor(255, 255, 255, 140));
    painter.fillPath(path, QBrush(gradient));
    QPen pen(QColor(255, 255, 255, 180), 1);
    painter.setPen(pen);
    painter.drawPath(path);
}

// ========== YesterdayCompare ==========

YesterdayCompare::YesterdayCompare(QWidget *parent)
    : QWidget(parent)
{
    setMinimumHeight(120);
}

void YesterdayCompare::setData(int todaySeconds, int yesterdaySeconds)
{
    m_todaySeconds = todaySeconds;
    m_yesterdaySeconds = yesterdaySeconds;
    update();
}

void YesterdayCompare::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    double w = width();
    double h = height();
    double cx = w / 2.0;

    int today = m_todaySeconds;
    int yesterday = m_yesterdaySeconds;

    if (today == 0 && yesterday == 0) {
        painter.setFont(appFont(13));
        painter.setPen(DesignTokens::kTextFaint());
        painter.drawText(QRectF(0, 0, w, h), Qt::AlignCenter,
                         QString::fromUtf8("\xe6\x9a\x82\xe6\x97\xa0\xe6\x95\xb0\xe6\x8d\xae"));
        return;
    }

    int todayMins = today / 60;
    int todayH = todayMins / 60;
    int todayRm = todayMins % 60;
    QString todayStr = (todayH > 0)
        ? QString("%1h %2m").arg(todayH).arg(todayRm)
        : QString("%1m").arg(todayMins);

    int yesterdayMins = yesterday / 60;
    int yesterdayH = yesterdayMins / 60;
    int yesterdayRm = yesterdayMins % 60;
    QString yesterdayStr = (yesterdayH > 0)
        ? QString("%1h %2m").arg(yesterdayH).arg(yesterdayRm)
        : QString("%1m").arg(yesterdayMins);

    double yOffset = 12.0;

    // --- Title ---
    painter.setFont(appFont(12));
    painter.setPen(DesignTokens::kTextMute());
    painter.drawText(QRectF(0, yOffset, w, 20), Qt::AlignCenter,
                     QString::fromUtf8("\xe8\xbe\x83\xe6\x98\xa8\xe6\x97\xa5"));
    yOffset += 28.0;

    // --- Percentage ---
    QColor pctColor;
    QString arrow;
    QString pctText;
    if (yesterday > 0) {
        double pct = (static_cast<double>(today - yesterday) / yesterday) * 100.0;
        int pctInt = static_cast<int>(std::abs(pct) + 0.5);
        if (pct > 0) {
            pctColor = DesignTokens::kSuccess();
            arrow = QString::fromUtf8("\xe2\x86\x91");
            pctText = QString("%1 %2%").arg(arrow).arg(pctInt);
        } else if (pct < 0) {
            pctColor = DesignTokens::kError();
            arrow = QString::fromUtf8("\xe2\x86\x93");
            pctText = QString("%1 %2%").arg(arrow).arg(pctInt);
        } else {
            pctColor = DesignTokens::kTextMute();
            arrow = QString::fromUtf8("\xe2\x86\x92");
            pctText = QString("%1 0%").arg(arrow);
        }
    } else {
        pctColor = DesignTokens::kAccent();
        pctText = QString::fromUtf8("\xe6\x96\xb0\xe5\xa2\x9e");
    }

    QFont pctFont = appFont(26, QFont::Bold);
    painter.setFont(pctFont);
    painter.setPen(pctColor);
    painter.drawText(QRectF(0, yOffset, w, 36), Qt::AlignCenter, pctText);
    yOffset += 40.0;

    // --- Today / Yesterday text ---
    double textY = yOffset;
    painter.setFont(appFont(13));
    painter.setPen(DesignTokens::kTextStrong());
    painter.drawText(QRectF(0, textY, w, 20), Qt::AlignCenter,
                     QString::fromUtf8("\xe4\xbb\x8a\xe6\x97\xa5 ") + todayStr);
    textY += 20.0;
    painter.setPen(DesignTokens::kTextMute());
    painter.drawText(QRectF(0, textY, w, 20), Qt::AlignCenter,
                     QString::fromUtf8("\xe6\x98\xa8\xe6\x97\xa5 ") + yesterdayStr);
    textY += 28.0;

    // --- Comparison bars ---
    int maxVal = qMax(today, yesterday);
    if (maxVal == 0) maxVal = 1;
    double barMaxW = w * 0.55;
    double barH = 8.0;
    double barCorner = 4.0;
    double barX = cx - barMaxW / 2.0;

    // Today bar
    double todayW = (static_cast<double>(today) / maxVal) * barMaxW;
    QPainterPath todayPath;
    todayPath.addRoundedRect(barX, textY, todayW, barH, barCorner, barCorner);
    painter.setPen(Qt::NoPen);
    painter.setBrush(DesignTokens::kAccent());
    painter.drawPath(todayPath);

    painter.setFont(appFont(10));
    painter.setPen(DesignTokens::kTextMute());
    painter.drawText(QRectF(barX + todayW + 6, textY - 2, 40, barH + 4),
                     Qt::AlignVCenter,
                     QString::fromUtf8("\xe4\xbb\x8a\xe6\x97\xa5"));
    textY += barH + 8.0;

    // Yesterday bar
    double yesterdayW = (static_cast<double>(yesterday) / maxVal) * barMaxW;
    QPainterPath yesterdayPath;
    yesterdayPath.addRoundedRect(barX, textY, yesterdayW, barH, barCorner, barCorner);
    painter.setBrush(DesignTokens::kCompareYesterdayBar());
    painter.drawPath(yesterdayPath);

    painter.setPen(DesignTokens::kTextMute());
    painter.drawText(QRectF(barX + yesterdayW + 6, textY - 2, 40, barH + 4),
                     Qt::AlignVCenter,
                     QString::fromUtf8("\xe6\x98\xa8\xe6\x97\xa5"));
}

// ========== StatsWidget ==========

StatsWidget::StatsWidget(DatabaseManager *db, QWidget *parent)
    : QWidget(parent), m_db(db)
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(20);

    // Header
    QLabel *header = new QLabel(QString::fromUtf8("\xe4\xbb\x8a\xe6\x97\xa5\xe6\xa6\x82\xe8\xa7\x88"), this);
    header->setFont(appFont(18, QFont::Medium));
    header->setStyleSheet(QString("color: %1;").arg(DesignTokens::kTextStrong().name()));
    mainLayout->addWidget(header);

    // Top row: 2 cards (today + top app)
    QHBoxLayout *cardsLayout = new QHBoxLayout();
    cardsLayout->setSpacing(16);

    GlassCard *todayCard = new GlassCard(this);
    QVBoxLayout *todayLayout = new QVBoxLayout(todayCard);
    todayLayout->setAlignment(Qt::AlignCenter);
    QLabel *todayTitle = new QLabel(QString::fromUtf8("\xe4\xbb\x8a\xe6\x97\xa5\xe6\x80\xbb\xe6\x97\xb6\xe9\x95\xbf"), todayCard);
    todayTitle->setFont(appFont(12));
    todayTitle->setStyleSheet(QString("color: %1;").arg(DesignTokens::kTextMute().name()));
    todayTitle->setAlignment(Qt::AlignCenter);
    m_bigNumber = new BigNumberWidget(todayCard);
    todayLayout->addWidget(todayTitle);
    todayLayout->addWidget(m_bigNumber, 0, Qt::AlignCenter);
    cardsLayout->addWidget(todayCard, 1);

    GlassCard *topAppCardContainer = new GlassCard(this);
    QVBoxLayout *topAppLayout = new QVBoxLayout(topAppCardContainer);
    topAppLayout->setAlignment(Qt::AlignCenter);
    QLabel *topAppTitle = new QLabel(QString::fromUtf8("\xe4\xbb\x8a\xe6\x97\xa5\xe6\x9c\x80\xe5\xb8\xb8\xe7\x94\xa8"), topAppCardContainer);
    topAppTitle->setFont(appFont(12));
    topAppTitle->setStyleSheet(QString("color: %1;").arg(DesignTokens::kTextMute().name()));
    topAppTitle->setAlignment(Qt::AlignCenter);
    m_topAppCard = new TopAppCard(topAppCardContainer);
    topAppLayout->addWidget(topAppTitle);
    topAppLayout->addWidget(m_topAppCard, 0, Qt::AlignCenter);
    cardsLayout->addWidget(topAppCardContainer, 1);

    mainLayout->addLayout(cardsLayout);

    // Bottom row: 2 cards side by side (daily trend + yesterday compare)
    QHBoxLayout *bottomRow = new QHBoxLayout();
    bottomRow->setSpacing(16);

    // --- Left card: Daily trend with chart toggle ---
    GlassCard *trendCard = new GlassCard(this);
    QVBoxLayout *trendLayout = new QVBoxLayout(trendCard);
    trendLayout->setContentsMargins(16, 12, 16, 12);
    trendLayout->setSpacing(8);

    // Title row with toggle buttons
    QHBoxLayout *titleRow = new QHBoxLayout();
    titleRow->setContentsMargins(0, 0, 0, 0);
    titleRow->setSpacing(6);

    QLabel *trendTitle = new QLabel(QString::fromUtf8("\xe6\xaf\x8f\xe6\x97\xa5\xe8\xb6\x8b\xe5\x8a\xbf"), trendCard);
    trendTitle->setFont(appFont(12));
    trendTitle->setStyleSheet(QString("color: %1;").arg(DesignTokens::kTextMute().name()));
    titleRow->addWidget(trendTitle);
    titleRow->addStretch();

    QPushButton *barBtn = new QPushButton(QString::fromUtf8("\xf0\x9f\x93\x8a"), trendCard);
    barBtn->setFixedSize(28, 28);
    barBtn->setCheckable(true);
    barBtn->setFlat(true);
    barBtn->setCursor(Qt::PointingHandCursor);

    QPushButton *lineBtn = new QPushButton(QString::fromUtf8("\xf0\x9f\x93\x88"), trendCard);
    lineBtn->setFixedSize(28, 28);
    lineBtn->setCheckable(true);
    lineBtn->setFlat(true);
    lineBtn->setCursor(Qt::PointingHandCursor);

    QButtonGroup *chartGroup = new QButtonGroup(trendCard);
    chartGroup->addButton(barBtn, 0);
    chartGroup->addButton(lineBtn, 1);
    chartGroup->setExclusive(true);

    titleRow->addWidget(barBtn);
    titleRow->addWidget(lineBtn);
    trendLayout->addLayout(titleRow);

    // Chart stack
    m_chartStack = new QStackedWidget(trendCard);
    m_weeklyBar = new WeeklyBar(m_chartStack);
    m_weeklyLine = new WeeklyLine(m_chartStack);
    m_chartStack->addWidget(m_weeklyBar);
    m_chartStack->addWidget(m_weeklyLine);
    trendLayout->addWidget(m_chartStack);

    // Read persisted chart type preference
    QString chartType = m_db->getSetting("chart_type", "bar");
    if (chartType == "line") {
        lineBtn->setChecked(true);
        m_chartStack->setCurrentIndex(1);
    } else {
        barBtn->setChecked(true);
        m_chartStack->setCurrentIndex(0);
    }

    // Toggle button style helper
    auto applyToggleStyle = [](QPushButton *btn, bool checked) {
        if (checked) {
            btn->setStyleSheet(
                QString("QPushButton { color: %1; background-color: %2; "
                        "border-radius: 6px; font-size: 14px; border: none; }")
                .arg(DesignTokens::kAccent().name(QColor::HexArgb))
                .arg(DesignTokens::kAccentGlow().name(QColor::HexArgb)));
        } else {
            btn->setStyleSheet(
                QString("QPushButton { color: %1; background-color: transparent; "
                        "border-radius: 6px; font-size: 14px; border: none; }"
                        "QPushButton:hover { background-color: %2; }")
                .arg(DesignTokens::kTextFaint().name(QColor::HexArgb))
                .arg(DesignTokens::kButtonHoverBg().name(QColor::HexArgb)));
        }
    };
    applyToggleStyle(barBtn, barBtn->isChecked());
    applyToggleStyle(lineBtn, lineBtn->isChecked());

    QObject::connect(chartGroup, QOverload<int>::of(&QButtonGroup::idClicked),
        [this, barBtn, lineBtn, applyToggleStyle](int id) {
            m_chartStack->setCurrentIndex(id);
            applyToggleStyle(barBtn, barBtn->isChecked());
            applyToggleStyle(lineBtn, lineBtn->isChecked());
            m_db->setSetting("chart_type", id == 1 ? "line" : "bar");
        });

    bottomRow->addWidget(trendCard, 3);

    // --- Right card: Yesterday comparison ---
    GlassCard *compareCard = new GlassCard(this);
    QVBoxLayout *compareLayout = new QVBoxLayout(compareCard);
    compareLayout->setContentsMargins(16, 12, 16, 12);
    m_yesterdayCompare = new YesterdayCompare(compareCard);
    compareLayout->addWidget(m_yesterdayCompare);
    bottomRow->addWidget(compareCard, 2);

    mainLayout->addLayout(bottomRow);
}

void StatsWidget::refresh()
{
    int todayTotal = m_db->getTodayTotal();
    m_bigNumber->setValue(todayTotal);

    QVector<QVariantMap> weekData = m_db->getWeekSummary();
    m_weeklyBar->setData(weekData);
    m_weeklyLine->setData(weekData);

    m_yesterdayCompare->setData(todayTotal, m_db->getYesterdayTotal());

    QVector<QVariantMap> rank = m_db->getAppRank();
    if (!rank.isEmpty()) {
        QIcon icon = AppIconProvider::instance()->icon(
            rank[0]["process_name"].toString(), 24);
        m_topAppCard->setApp(
            rank[0]["app_name"].toString(),
            rank[0]["total_seconds"].toInt(), icon);
    } else {
        m_topAppCard->setApp(QString::fromUtf8("\xe6\x9a\x82\xe6\x97\xa0\xe6\x95\xb0\xe6\x8d\xae"), 0);
    }
}
