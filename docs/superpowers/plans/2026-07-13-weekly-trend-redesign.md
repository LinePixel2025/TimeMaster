# 本周趋势重新设计 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将 StatsWidget 中单一的"本周趋势" GlassCard 重构为左右双卡片分栏布局 — 左侧可切换柱状图/折线图的"每日趋势"，右侧"较昨日"使用时长对比。

**Architecture:** 在现有 `stats_widget.h/.cpp` 中新增 `WeeklyLine`（折线面积图）和 `YesterdayCompare`（昨日对比）两个 QWidget 子类。StatsWidget::refresh() 新增 `getYesterdayTotal()` 调用。图表切换通过 QStackedWidget + 两个 checkable QPushButton 实现，偏好存入 settings 表。

**Tech Stack:** C++17, Qt6 Widgets, QPainter 自定义绘制, SQLite via Qt6::Sql

## Global Constraints

- 字体：`Microsoft YaHei`，通过本地 `appFont()` 辅助函数获取
- 颜色：靛蓝系渐变 `#A5B4FC → #6366F1`，背景 `#F0F2F5`，卡片 `rgba(255,255,255,190)→rgba(255,255,255,140)`
- 线程安全：所有 `m_db` 访问必须获取 `QMutexLocker lock(&m_mutex)`
- 无透明背景：禁止 `WA_TranslucentBackground`、`DwmSetWindowAttribute(DWMWA_SYSTEMBACKDROP_TYPE)`
- 构建命令：`.\build_mingw.ps1`（MinGW/Ninja）

---

### Task 1: Add `getYesterdayTotal()` to DatabaseManager

**Files:**
- Modify: `src/database/database_manager.h:28`（在 `getTodayTotal()` 声明后插入）
- Modify: `src/database/database_manager.cpp:142`（在 `getTodayTotal()` 实现后插入）

**Interfaces:**
- Produces: `int DatabaseManager::getYesterdayTotal()`

- [ ] **Step 1: Add declaration to database_manager.h**

在 `int getTodayTotal();` 之后插入：

```cpp
int getYesterdayTotal();
```

- [ ] **Step 2: Add implementation to database_manager.cpp**

在 `getTodayTotal()` 的 `}` 之后插入：

```cpp
int DatabaseManager::getYesterdayTotal()
{
    QMutexLocker lock(&m_mutex);
    QSqlQuery q(m_db);
    q.prepare("SELECT COALESCE(SUM(duration_seconds), 0) as total "
              "FROM sessions WHERE date(start_time) = ?");
    q.addBindValue(QDate::currentDate().addDays(-1).toString(Qt::ISODate));
    q.exec();
    if (q.next())
        return q.value("total").toInt();
    return 0;
}
```

- [ ] **Step 3: Build to verify compilation**

```powershell
$env:PATH = "D:\AICOP\requirements\QT6\Tools\mingw1310_64\bin;D:\AICOP\requirements\QT6\6.11.1\mingw_64\bin;D:\AICOP\requirements\QT6\Tools\Ninja;D:\AICOP\requirements\QT6\Tools\CMake_64\bin;$env:PATH"
cmake --preset mingw
cmake --build build
```

Expected: 构建成功，无编译错误。

- [ ] **Step 4: Commit**

```bash
git add src/database/database_manager.h src/database/database_manager.cpp
git commit -m "feat: add getYesterdayTotal() to DatabaseManager"
```

---

### Task 2: Implement `YesterdayCompare` Widget

**Files:**
- Modify: `src/ui/stats_widget.h:56`（在 `TopAppCard` 类声明之后插入新类）
- Modify: `src/ui/stats_widget.cpp:267`（在 `TopAppCard` 实现之后插入新实现）

**Interfaces:**
- Consumes: `int getYesterdayTotal()` from Task 1
- Produces: `class YesterdayCompare : public QWidget` with `void setData(int todaySeconds, int yesterdaySeconds)`

- [ ] **Step 1: Add class declaration to stats_widget.h**

在 `TopAppCard` 类的 `};` 之后、`StatsWidget` 类声明之前插入：

```cpp
class YesterdayCompare : public QWidget
{
    Q_OBJECT
public:
    explicit YesterdayCompare(QWidget *parent = nullptr);
    void setData(int todaySeconds, int yesterdaySeconds);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    int m_todaySeconds = 0;
    int m_yesterdaySeconds = 0;
};
```

- [ ] **Step 2: Add implementation to stats_widget.cpp**

在 `TopAppCard` 实现区域的 `}` 之后（约第 267 行）、`// ========== StatsWidget ==========` 注释之前插入：

```cpp
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
        painter.setPen(QColor("#9CA3AF"));
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
    painter.setPen(QColor("#6B7280"));
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
            pctColor = QColor("#10B981");
            arrow = QString::fromUtf8("\xe2\x86\x91");
            pctText = QString("%1 %2%").arg(arrow).arg(pctInt);
        } else if (pct < 0) {
            pctColor = QColor("#EF4444");
            arrow = QString::fromUtf8("\xe2\x86\x93");
            pctText = QString("%1 %2%").arg(arrow).arg(pctInt);
        } else {
            pctColor = QColor("#6B7280");
            arrow = QString::fromUtf8("\xe2\x86\x92");
            pctText = QString("%1 0%").arg(arrow);
        }
    } else {
        pctColor = QColor("#6366F1");
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
    painter.setPen(QColor("#1F2937"));
    painter.drawText(QRectF(0, textY, w, 20), Qt::AlignCenter,
                     QString::fromUtf8("\xe4\xbb\x8a\xe6\x97\xa5 ") + todayStr);
    textY += 20.0;
    painter.setPen(QColor("#6B7280"));
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
    painter.setBrush(QColor("#6366F1"));
    painter.drawPath(todayPath);

    painter.setFont(appFont(10));
    painter.setPen(QColor("#6B7280"));
    painter.drawText(QRectF(barX + todayW + 6, textY - 2, 40, barH + 4),
                     Qt::AlignVCenter,
                     QString::fromUtf8("\xe4\xbb\x8a\xe6\x97\xa5"));
    textY += barH + 8.0;

    // Yesterday bar
    double yesterdayW = (static_cast<double>(yesterday) / maxVal) * barMaxW;
    QPainterPath yesterdayPath;
    yesterdayPath.addRoundedRect(barX, textY, yesterdayW, barH, barCorner, barCorner);
    painter.setBrush(QColor("#D1D5DB"));
    painter.drawPath(yesterdayPath);

    painter.setPen(QColor("#6B7280"));
    painter.drawText(QRectF(barX + yesterdayW + 6, textY - 2, 40, barH + 4),
                     Qt::AlignVCenter,
                     QString::fromUtf8("\xe6\x98\xa8\xe6\x97\xa5"));
}
```

- [ ] **Step 3: Build to verify compilation**

```powershell
$env:PATH = "D:\AICOP\requirements\QT6\Tools\mingw1310_64\bin;D:\AICOP\requirements\QT6\6.11.1\mingw_64\bin;D:\AICOP\requirements\QT6\Tools\Ninja;D:\AICOP\requirements\QT6\Tools\CMake_64\bin;$env:PATH"
cmake --build build
```

Expected: 构建成功。

- [ ] **Step 4: Commit**

```bash
git add src/ui/stats_widget.h src/ui/stats_widget.cpp
git commit -m "feat: add YesterdayCompare widget for today-vs-yesterday comparison"
```

---

### Task 3: Implement `WeeklyLine` Widget

**Files:**
- Modify: `src/ui/stats_widget.h:55`（在 `WeeklyBar` 类声明之后插入）
- Modify: `src/ui/stats_widget.cpp:217`（在 `WeeklyBar` 实现之后插入）

**Interfaces:**
- Produces: `class WeeklyLine : public QWidget` with `void setData(const QVector<QVariantMap> &weekData)`

- [ ] **Step 1: Add class declaration to stats_widget.h**

在 `WeeklyBar` 类的 `};` 之后插入：

```cpp
class WeeklyLine : public QWidget
{
    Q_OBJECT
public:
    explicit WeeklyLine(QWidget *parent = nullptr);
    void setData(const QVector<QVariantMap> &weekData);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QMap<QString, int> m_data;
    int m_maxVal = 1;
};
```

- [ ] **Step 2: Add implementation to stats_widget.cpp**

在 `WeeklyBar` 实现区域的 `}` 之后（约第 217 行）、`// ========== TopAppCard ==========` 注释之前插入：

```cpp
// ========== WeeklyLine ==========

WeeklyLine::WeeklyLine(QWidget *parent)
    : QWidget(parent)
{
    setFixedHeight(120);
}

void WeeklyLine::setData(const QVector<QVariantMap> &weekData)
{
    QStringList dayNames = {
        QString::fromUtf8("\xe4\xb8\x80"),
        QString::fromUtf8("\xe4\xba\x8c"),
        QString::fromUtf8("\xe4\xb8\x89"),
        QString::fromUtf8("\xe5\x9b\x9b"),
        QString::fromUtf8("\xe4\xba\x94"),
        QString::fromUtf8("\xe5\x85\xad"),
        QString::fromUtf8("\xe6\x97\xa5")
    };

    QDate today = QDate::currentDate();
    QDate monday = today.addDays(-today.dayOfWeek() + 1);
    m_data.clear();
    m_maxVal = 1;

    for (int i = 0; i < 7; ++i) {
        m_data[monday.addDays(i).toString(Qt::ISODate)] = 0;
    }
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

    double w = width();
    double h = height();
    int pointCount = 7;
    double margin = 24.0;
    double chartW = w - margin * 2;
    double stepX = chartW / (pointCount - 1);
    double labelY = h - 25;
    double chartTop = 20.0;
    double chartAreaH = labelY - 30 - chartTop;

    QStringList dayNames = {
        QString::fromUtf8("\xe4\xb8\x80"),
        QString::fromUtf8("\xe4\xba\x8c"),
        QString::fromUtf8("\xe4\xb8\x89"),
        QString::fromUtf8("\xe5\x9b\x9b"),
        QString::fromUtf8("\xe4\xba\x94"),
        QString::fromUtf8("\xe5\x85\xad"),
        QString::fromUtf8("\xe6\x97\xa5")
    };
    QDate today = QDate::currentDate();
    QDate monday = today.addDays(-today.dayOfWeek() + 1);

    // --- Collect data points ---
    QVector<QPointF> points;
    for (int i = 0; i < pointCount; ++i) {
        double x = margin + i * stepX;
        QString d = monday.addDays(i).toString(Qt::ISODate);
        int val = m_data.value(d, 0);
        double barH = (static_cast<double>(val) / m_maxVal) * chartAreaH;
        double y = labelY - 30 - barH;
        points.append(QPointF(x, y));
    }

    // --- Area fill path ---
    QPainterPath areaPath;
    areaPath.moveTo(points[0]);

    // Smooth cubic bezier through points
    for (int i = 0; i < pointCount - 1; ++i) {
        QPointF cp1 = QPointF((points[i].x() + points[i + 1].x()) / 2.0, points[i].y());
        QPointF cp2 = QPointF((points[i].x() + points[i + 1].x()) / 2.0, points[i + 1].y());
        areaPath.cubicTo(cp1, cp2, points[i + 1]);
    }

    // Close area: line to bottom-right, then bottom-left
    areaPath.lineTo(points.last().x(), labelY - 30);
    areaPath.lineTo(points.first().x(), labelY - 30);
    areaPath.closeSubpath();

    // Area fill gradient
    QLinearGradient areaGrad(0, chartTop, 0, labelY - 30);
    areaGrad.setColorAt(0.0, QColor(165, 180, 252, 100));
    areaGrad.setColorAt(1.0, QColor(165, 180, 252, 0));
    painter.setBrush(areaGrad);
    painter.setPen(Qt::NoPen);
    painter.drawPath(areaPath);

    // --- Smooth line ---
    QPainterPath linePath;
    linePath.moveTo(points[0]);
    for (int i = 0; i < pointCount - 1; ++i) {
        QPointF cp1 = QPointF((points[i].x() + points[i + 1].x()) / 2.0, points[i].y());
        QPointF cp2 = QPointF((points[i].x() + points[i + 1].x()) / 2.0, points[i + 1].y());
        linePath.cubicTo(cp1, cp2, points[i + 1]);
    }

    QPen linePen(QColor("#6366F1"), 2.0);
    linePen.setCapStyle(Qt::RoundCap);
    linePen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(linePen);
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(linePath);

    // --- Data point dots ---
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor("#6366F1"));
    for (int i = 0; i < pointCount; ++i) {
        QString d = monday.addDays(i).toString(Qt::ISODate);
        int val = m_data.value(d, 0);
        if (val > 0) {
            painter.drawEllipse(points[i], 3.5, 3.5);
        }
    }

    // --- Value labels above points ---
    painter.setFont(appFont(8));
    for (int i = 0; i < pointCount; ++i) {
        QString d = monday.addDays(i).toString(Qt::ISODate);
        int val = m_data.value(d, 0);
        if (val > 0) {
            int hours = val / 3600;
            int mins = (val % 3600) / 60;
            QString label = (hours > 0)
                ? QString("%1h%2").arg(hours).arg(mins, 2, 10, QChar('0'))
                : QString("%1m").arg(mins);
            painter.setPen(QColor("#374151"));
            painter.drawText(QRectF(points[i].x() - 18, points[i].y() - 16, 36, 14),
                             Qt::AlignCenter, label);
        }
    }

    // --- Day labels ---
    painter.setFont(appFont(10));
    for (int i = 0; i < pointCount; ++i) {
        double x = margin + i * stepX;
        painter.setPen(QColor("#4B5563"));
        painter.drawText(QRectF(x - 16, labelY - 5, 32, 20), Qt::AlignCenter, dayNames[i]);
    }
}
```

- [ ] **Step 3: Build to verify compilation**

```powershell
$env:PATH = "D:\AICOP\requirements\QT6\Tools\mingw1310_64\bin;D:\AICOP\requirements\QT6\6.11.1\mingw_64\bin;D:\AICOP\requirements\QT6\Tools\Ninja;D:\AICOP\requirements\QT6\Tools\CMake_64\bin;$env:PATH"
cmake --build build
```

Expected: 构建成功。

- [ ] **Step 4: Commit**

```bash
git add src/ui/stats_widget.h src/ui/stats_widget.cpp
git commit -m "feat: add WeeklyLine widget for area chart visualization"
```

---

### Task 4: Refactor StatsWidget Layout with Chart Toggle

**Files:**
- Modify: `src/ui/stats_widget.h:70-82`（StatsWidget 类成员声明）
- Modify: `src/ui/stats_widget.cpp:269-341`（StatsWidget 构造函数和 refresh()）

**Interfaces:**
- Consumes: `YesterdayCompare` from Task 2, `WeeklyLine` from Task 3, `getYesterdayTotal()` from Task 1
- Produces: Updated `StatsWidget` with new layout and chart toggle

**New includes needed in stats_widget.h:**
```cpp
#include <QStackedWidget>
```
**New includes needed in stats_widget.cpp:**
```cpp
#include <QPushButton>
#include <QButtonGroup>
```

- [ ] **Step 1: Update stats_widget.h includes and StatsWidget members**

在 `#include <QIcon>` 之后添加：

```cpp
#include <QStackedWidget>
```

修改 StatsWidget 私有成员区域（约第 77-82 行），从：

```cpp
private:
    DatabaseManager *m_db;
    CircularProgress *m_circularProgress;
    WeeklyBar *m_weeklyBar;
    TopAppCard *m_topAppCard;
```

改为：

```cpp
private:
    DatabaseManager *m_db;
    CircularProgress *m_circularProgress;
    WeeklyBar *m_weeklyBar;
    WeeklyLine *m_weeklyLine;
    QStackedWidget *m_chartStack;
    TopAppCard *m_topAppCard;
    YesterdayCompare *m_yesterdayCompare;
```

- [ ] **Step 2: Update stats_widget.cpp includes**

在现有 include 区域末尾（`#include <QtMath>` 之后）添加：

```cpp
#include <QStackedWidget>
#include <QPushButton>
#include <QButtonGroup>
```

- [ ] **Step 3: Replace StatsWidget constructor**

将第 271-324 行的完整 `StatsWidget::StatsWidget()` 构造函数替换为：

```cpp
StatsWidget::StatsWidget(DatabaseManager *db, QWidget *parent)
    : QWidget(parent), m_db(db)
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(20);

    // Header
    QLabel *header = new QLabel(QString::fromUtf8("\xe4\xbb\x8a\xe6\x97\xa5\xe6\xa6\x82\xe8\xa7\x88"), this);
    header->setFont(appFont(18, QFont::Medium));
    header->setStyleSheet("color: #1F2937;");
    mainLayout->addWidget(header);

    // Top row: 2 cards (today + top app)
    QHBoxLayout *cardsLayout = new QHBoxLayout();
    cardsLayout->setSpacing(16);

    GlassCard *todayCard = new GlassCard(this);
    QVBoxLayout *todayLayout = new QVBoxLayout(todayCard);
    todayLayout->setAlignment(Qt::AlignCenter);
    QLabel *todayTitle = new QLabel(QString::fromUtf8("\xe4\xbb\x8a\xe6\x97\xa5\xe6\x80\xbb\xe6\x97\xb6\xe9\x95\xbf"), todayCard);
    todayTitle->setFont(appFont(12));
    todayTitle->setStyleSheet("color: #6B7280;");
    todayTitle->setAlignment(Qt::AlignCenter);
    m_circularProgress = new CircularProgress(todayCard);
    todayLayout->addWidget(todayTitle);
    todayLayout->addWidget(m_circularProgress, 0, Qt::AlignCenter);
    cardsLayout->addWidget(todayCard, 1);

    GlassCard *topAppCardContainer = new GlassCard(this);
    QVBoxLayout *topAppLayout = new QVBoxLayout(topAppCardContainer);
    topAppLayout->setAlignment(Qt::AlignCenter);
    QLabel *topAppTitle = new QLabel(QString::fromUtf8("\xe4\xbb\x8a\xe6\x97\xa5\xe6\x9c\x80\xe5\xb8\xb8\xe7\x94\xa8"), topAppCardContainer);
    topAppTitle->setFont(appFont(12));
    topAppTitle->setStyleSheet("color: #6B7280;");
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
    trendTitle->setStyleSheet("color: #6B7280;");
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
                "QPushButton { color: #6366F1; background-color: rgba(99,102,241,0.1); "
                "border-radius: 6px; font-size: 14px; border: none; }");
        } else {
            btn->setStyleSheet(
                "QPushButton { color: #9CA3AF; background-color: transparent; "
                "border-radius: 6px; font-size: 14px; border: none; }"
                "QPushButton:hover { background-color: rgba(0,0,0,0.04); }");
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
```

- [ ] **Step 4: Update StatsWidget::refresh()**

将第 326-341 行的 `refresh()` 方法替换为：

```cpp
void StatsWidget::refresh()
{
    int todayTotal = m_db->getTodayTotal();
    m_circularProgress->setValue(todayTotal);

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
```

- [ ] **Step 5: Build to verify compilation**

```powershell
$env:PATH = "D:\AICOP\requirements\QT6\Tools\mingw1310_64\bin;D:\AICOP\requirements\QT6\6.11.1\mingw_64\bin;D:\AICOP\requirements\QT6\Tools\Ninja;D:\AICOP\requirements\QT6\Tools\CMake_64\bin;$env:PATH"
cmake --build build
```

Expected: 构建成功，无编译错误。

- [ ] **Step 6: Commit**

```bash
git add src/ui/stats_widget.h src/ui/stats_widget.cpp
git commit -m "feat: redesign weekly trend with left-right split layout and chart toggle"
```

---

### Task 5: Build Full Project and Verify

- [ ] **Step 1: Kill any running instance**

```powershell
taskkill /f /im TimeMaster.exe 2>$null
```

- [ ] **Step 2: Full rebuild**

```powershell
$env:PATH = "D:\AICOP\requirements\QT6\Tools\mingw1310_64\bin;D:\AICOP\requirements\QT6\6.11.1\mingw_64\bin;D:\AICOP\requirements\QT6\Tools\Ninja;D:\AICOP\requirements\QT6\Tools\CMake_64\bin;$env:PATH"
cmake --build build
```

Expected: 构建成功，无警告。

- [ ] **Step 3: Run tests**

```powershell
$env:PATH = "D:\AICOP\requirements\QT6\Tools\mingw1310_64\bin;D:\AICOP\requirements\QT6\6.11.1\mingw_64\bin;D:\AICOP\requirements\QT6\Tools\Ninja;D:\AICOP\requirements\QT6\Tools\CMake_64\bin;$env:PATH"
$env:QT_PLUGIN_PATH = "D:\AICOP\requirements\QT6\6.11.1\mingw_64\plugins"
.\build\tests\test_database.exe
.\build\tests\test_exporter.exe
```

Expected: 两个测试均返回 0。

- [ ] **Step 4: Visual verification**

临时在 `main.cpp` 中添加 `window.show()`（搜索 `window.show()` 确认当前状态），构建后用截图验证 UI：
- 左侧"每日趋势"卡片显示柱状图，默认选中 📊 按钮
- 右侧"较昨日"卡片正常显示
- 点击 📈 切换到折线面积图
- 切换后偏好持久化（关闭再打开保持选择）
- 验证后恢复 `main.cpp`

- [ ] **Step 5: Commit any fixes**

```bash
git add .
git commit -m "chore: final verification and fixes for weekly trend redesign"
```
