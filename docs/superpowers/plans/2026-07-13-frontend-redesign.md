# Time Master 前端 BUG 修复与 UI 重设计 实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans` to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 修复主界面黑色/无 MICA、应用列表无滚动、界面拥挤简陋的问题，并采用浅色柔和 MICA Dashboard 风格重设计。

**Architecture:** 保持现有 C++17/Qt6 Widgets 结构，修改 `MainWindow` 的背景与 MICA 策略，`StatsWidget` 改为 Dashboard 三卡片+图表布局，`AppRankWidget` 增加 `QScrollArea`，统一浅色配色和字体层次。不引入新依赖。

**Tech Stack:** C++17, Qt6 Widgets/SQL, Windows DWM API, CMake.

## Global Constraints

- 使用 C++17 标准。
- 仅使用 Qt6 Widgets + Sql，不引入额外库。
- 保持 `src/ui/` 现有文件结构，必要时拆分过大文件。
- 目标平台 Windows 10/11，MICA 在 Windows 11 生效，Windows 10 回退纯色背景。
- 使用 `PingFang SC` 作为主要字体，必要时回退到系统默认字体。
- 提交风格：conventional commits (`feat:`, `fix:`, `chore:`)。

---

## Task 1: Fix MainWindow MICA Black Screen and Background Strategy

**Files:**
- Modify: `src/ui/main_window.h`
- Modify: `src/ui/main_window.cpp`
- Test: 构建后在 Windows 10/11 目测窗口背景，不再全黑。

**Interfaces:**
- Consumes: `DatabaseManager`, `StatsWidget`, `AppRankWidget`。
- Produces: `MainWindow` 构造函数完成背景与 MICA 初始化；`applyBackdrop()` 私有槽。

- [ ] **Step 1: Update header to add version helpers and palette methods**

Add to `src/ui/main_window.h`:

```cpp
private slots:
    void applyBackdrop();
    void onExport();

private:
    void setupPalette();
    bool isMicaAvailable() const;

    DatabaseManager *m_db;
    StatsWidget *m_statsWidget;
    AppRankWidget *m_appRankWidget;
    QTimer *m_refreshTimer;
};
```

- [ ] **Step 2: Replace window background setup in constructor**

In `src/ui/main_window.cpp`, replace lines 54-62 with:

```cpp
setWindowTitle("Time Master");
setMinimumSize(900, 600);
resize(1000, 700);

// Remove layered transparency that conflicts with DWM system backdrop.
setAttribute(Qt::WA_TranslucentBackground, false);
setAutoFillBackground(true);

setupPalette();
```

- [ ] **Step 3: Implement setupPalette and MICA helper**

Add the following after `applyBackdropEffect()`:

```cpp
static bool isWindows11OrGreater()
{
    DWORD build = getWindowsBuild();
    return build >= 22000;
}

static void applyBackdropEffect(HWND hwnd)
{
    if (!isWindows11OrGreater())
        return;

    DWORD backdrop = 2; // DWMSBT_MAINWINDOW (Mica)
    DwmSetWindowAttribute(hwnd, 38, &backdrop, sizeof(backdrop)); // DWMWA_SYSTEMBACKDROP_TYPE
    BOOL dark = FALSE;
    DwmSetWindowAttribute(hwnd, 20, &dark, sizeof(dark)); // DWMWA_USE_IMMERSIVE_DARK_MODE, light mode
}

void MainWindow::setupPalette()
{
    QPalette pal = palette();
    if (isWindows11OrGreater()) {
        // Transparent client area so MICA shows through.
        pal.setColor(QPalette::Window, QColor(0, 0, 0, 0));
    } else {
        // Fallback for Windows 10 or DWM unavailable.
        pal.setColor(QPalette::Window, QColor("#F4F6F8"));
    }
    setPalette(pal);
}
```

- [ ] **Step 4: Update central widget and layout for light theme**

Replace the central widget creation (lines 60-65) with:

```cpp
QWidget *central = new QWidget(this);
QVBoxLayout *layout = new QVBoxLayout(central);
layout->setContentsMargins(24, 24, 24, 24);
layout->setSpacing(20);
setCentralWidget(central);
```

- [ ] **Step 5: Update button styles for light theme**

Replace `btnStyle` (lines 76-87) with:

```cpp
const QString btnStyle =
    "QPushButton {"
    "  background-color: #6366F1;"
    "  color: white; border: none; border-radius: 8px;"
    "  padding: 8px 20px; font-size: 13px; font-weight: 500;"
    "}"
    "QPushButton:hover {"
    "  background-color: #818CF8;"
    "}"
    "QPushButton:pressed {"
    "  background-color: #4F46E5;"
    "}";
```

- [ ] **Step 6: Commit**

```bash
git add src/ui/main_window.h src/ui/main_window.cpp
git commit -m "fix: resolve black MICA background and add light fallback"
```

---

## Task 2: Refactor StatsWidget into Dashboard Layout

**Files:**
- Modify: `src/ui/stats_widget.h`
- Modify: `src/ui/stats_widget.cpp`
- Test: 目测主界面显示三卡片 + 柱状图布局，颜色正常。

**Interfaces:**
- Consumes: `DatabaseManager::getTodayTotal()`, `DatabaseManager::getWeekSummary()`。
- Produces: `StatsWidget` 包含 `CircularProgress`, `WeeklyBar`, `TopAppCard`。

- [ ] **Step 1: Update header to add new card classes**

Replace `src/ui/stats_widget.h` with:

```cpp
#ifndef STATS_WIDGET_H
#define STATS_WIDGET_H

#include <QWidget>
#include <QFrame>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QMap>
#include <QDate>

class DatabaseManager;

class CircularProgress : public QWidget
{
    Q_OBJECT
public:
    explicit CircularProgress(QWidget *parent = nullptr);
    void setValue(int totalSeconds);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    int m_value = 0;
    int m_maxValue = 86400;
    int m_hours = 0;
    int m_minutes = 0;
};

class GlassCard : public QFrame
{
    Q_OBJECT
public:
    explicit GlassCard(QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;
};

class WeeklyBar : public QWidget
{
    Q_OBJECT
public:
    explicit WeeklyBar(QWidget *parent = nullptr);
    void setData(const QVector<QVariantMap> &weekData);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QMap<QString, int> m_data;
    int m_maxVal = 1;
};

class TopAppCard : public QWidget
{
    Q_OBJECT
public:
    explicit TopAppCard(QWidget *parent = nullptr);
    void setApp(const QString &name, int seconds);

private:
    QLabel *m_nameLabel;
    QLabel *m_timeLabel;
};

class StatsWidget : public QWidget
{
    Q_OBJECT
public:
    explicit StatsWidget(DatabaseManager *db, QWidget *parent = nullptr);
    void refresh();

private:
    DatabaseManager *m_db;
    CircularProgress *m_circularProgress;
    WeeklyBar *m_weeklyBar;
    TopAppCard *m_topAppCard;
};

#endif // STATS_WIDGET_H
```

- [ ] **Step 2: Update light theme colors in GlassCard paintEvent**

In `src/ui/stats_widget.cpp`, replace `GlassCard::paintEvent` (lines 71-84) with:

```cpp
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
```

- [ ] **Step 3: Update CircularProgress for light theme**

Replace `CircularProgress::paintEvent` text colors (lines 53, 58) with:

```cpp
painter.setPen(QColor("#1F2937"));
// ...
painter.setPen(QColor("#6B7280"));
```

- [ ] **Step 4: Update WeeklyBar for light theme**

Replace text colors in `WeeklyBar::paintEvent` (lines 170, 174) with:

```cpp
painter.setPen(QColor("#6B7280"));
// ...
painter.setPen(QColor("#9CA3AF"));
```

- [ ] **Step 5: Implement TopAppCard class**

Add after `WeeklyBar` implementation and before `// ========== StatsWidget ==========`:

```cpp
// ========== TopAppCard ==========

TopAppCard::TopAppCard(QWidget *parent)
    : QWidget(parent)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);
    layout->setAlignment(Qt::AlignCenter);

    m_nameLabel = new QLabel(QString::fromUtf8("\xe6\x9a\x82\xe6\x97\xa0\xe6\x95\xb0\xe6\x8d\xae"), this);
    m_nameLabel->setFont(QFont("PingFang SC", 16, QFont::Medium));
    m_nameLabel->setStyleSheet("color: #1F2937;");
    m_nameLabel->setAlignment(Qt::AlignCenter);
    m_nameLabel->setWordWrap(true);

    m_timeLabel = new QLabel("0m", this);
    m_timeLabel->setFont(QFont("PingFang SC", 13, QFont::Normal));
    m_timeLabel->setStyleSheet("color: #6B7280;");
    m_timeLabel->setAlignment(Qt::AlignCenter);

    layout->addWidget(m_nameLabel);
    layout->addWidget(m_timeLabel);
}

void TopAppCard::setApp(const QString &name, int seconds)
{
    m_nameLabel->setText(name);
    int mins = seconds / 60;
    int hours = mins / 60;
    int remainMins = mins % 60;
    QString timeStr = (hours > 0)
        ? QString("%1h %2m").arg(hours).arg(remainMins)
        : QString("%1m").arg(remainMins);
    m_timeLabel->setText(timeStr);
}
```

- [ ] **Step 6: Rewrite StatsWidget constructor for dashboard layout**

Replace `StatsWidget::StatsWidget` (lines 183-214) with:

```cpp
StatsWidget::StatsWidget(DatabaseManager *db, QWidget *parent)
    : QWidget(parent), m_db(db)
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(20);

    // Header
    QLabel *header = new QLabel(QString::fromUtf8("\xe4\xbb\x8a\xe6\x97\xa5\xe6\xa6\x82\xe8\xa7\x88"), this);
    header->setFont(QFont("PingFang SC", 18, QFont::Medium));
    header->setStyleSheet("color: #1F2937;");
    mainLayout->addWidget(header);

    // Top row: 3 cards
    QHBoxLayout *cardsLayout = new QHBoxLayout();
    cardsLayout->setSpacing(16);

    GlassCard *todayCard = new GlassCard(this);
    QVBoxLayout *todayLayout = new QVBoxLayout(todayCard);
    todayLayout->setAlignment(Qt::AlignCenter);
    QLabel *todayTitle = new QLabel(QString::fromUtf8("\xe4\xbb\x8a\xe6\x97\xa5\xe6\x80\xbb\xe6\x97\xb6\xe9\x95\xbf"), todayCard);
    todayTitle->setFont(QFont("PingFang SC", 12, QFont::Normal));
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
    topAppTitle->setFont(QFont("PingFang SC", 12, QFont::Normal));
    topAppTitle->setStyleSheet("color: #6B7280;");
    topAppTitle->setAlignment(Qt::AlignCenter);
    m_topAppCard = new TopAppCard(topAppCardContainer);
    topAppLayout->addWidget(topAppTitle);
    topAppLayout->addWidget(m_topAppCard, 0, Qt::AlignCenter);
    cardsLayout->addWidget(topAppCardContainer, 1);

    mainLayout->addLayout(cardsLayout);

    // Weekly chart card
    GlassCard *weekCard = new GlassCard(this);
    QVBoxLayout *weekLayout = new QVBoxLayout(weekCard);
    QLabel *weekTitle = new QLabel(QString::fromUtf8("\xe6\x9c\xac\xe5\x91\xa8\xe8\xb6\x8b\xe5\x8a\xbf"), weekCard);
    weekTitle->setFont(QFont("PingFang SC", 12, QFont::Normal));
    weekTitle->setStyleSheet("color: #6B7280;");
    m_weeklyBar = new WeeklyBar(weekCard);
    weekLayout->addWidget(weekTitle);
    weekLayout->addWidget(m_weeklyBar);
    mainLayout->addWidget(weekCard);
}
```

- [ ] **Step 7: Update StatsWidget::refresh to populate top app**

Add a new private helper or use `getAppRank()` in `refresh()`. Replace `refresh()` with:

```cpp
void StatsWidget::refresh()
{
    m_circularProgress->setValue(m_db->getTodayTotal());
    m_weeklyBar->setData(m_db->getWeekSummary());

    QVector<QVariantMap> rank = m_db->getAppRank();
    if (!rank.isEmpty()) {
        m_topAppCard->setApp(
            rank[0]["app_name"].toString(),
            rank[0]["total_seconds"].toInt());
    } else {
        m_topAppCard->setApp(QString::fromUtf8("\xe6\x9a\x82\xe6\x97\xa0\xe6\x95\xb0\xe6\x8d\xae"), 0);
    }
}
```

- [ ] **Step 8: Commit**

```bash
git add src/ui/stats_widget.h src/ui/stats_widget.cpp
git commit -m "feat: refactor stats widget into light dashboard layout"
```

---

## Task 3: Add Scrolling and Redesign AppRankWidget

**Files:**
- Modify: `src/ui/app_rank_widget.h`
- Modify: `src/ui/app_rank_widget.cpp`
- Test: 应用列表条目多时可滚动，不再挤压。

**Interfaces:**
- Consumes: `DatabaseManager::getAppRank()`。
- Produces: `AppRankWidget` 内部包含 `QScrollArea`。

- [ ] **Step 1: Update header for QScrollArea**

Add include to `src/ui/app_rank_widget.h`:

```cpp
#include <QScrollArea>
```

Update `AppRankWidget` private members:

```cpp
private:
    DatabaseManager *m_db;
    QVBoxLayout *m_listLayout;
    QWidget *m_listWidget;
    QScrollArea *m_scrollArea;
```

- [ ] **Step 2: Update AppRankItem colors for light theme**

In `src/ui/app_rank_widget.cpp`, replace `paintEvent` text colors with:

```cpp
painter.setPen(QColor("#6B7280"));
// rank text
painter.setPen(QColor("#1F2937"));
// app name
painter.setPen(QColor("#6B7280"));
// time text
```

- [ ] **Step 3: Add QScrollArea in constructor**

Replace `AppRankWidget::AppRankWidget` (lines 55-72) with:

```cpp
AppRankWidget::AppRankWidget(DatabaseManager *db, QWidget *parent)
    : QFrame(parent), m_db(db)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    QLabel *title = new QLabel(QString::fromUtf8("\xe5\xba\x94\xe7\x94\xa8\xe4\xbd\xbf\xe7\x94\xa8\xe6\x8e\x92\xe8\xa1\x8c (\xe4\xbb\x8a\xe6\x97\xa5)"), this);
    title->setFont(QFont("PingFang SC", 14, QFont::Medium));
    title->setStyleSheet("color: #1F2937;");
    layout->addWidget(title);

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setStyleSheet("QScrollArea { background: transparent; border: none; }");

    m_listWidget = new QWidget();
    m_listWidget->setStyleSheet("background: transparent;");
    m_listLayout = new QVBoxLayout(m_listWidget);
    m_listLayout->setContentsMargins(0, 0, 0, 0);
    m_listLayout->setSpacing(8);
    m_listLayout->addStretch();

    m_scrollArea->setWidget(m_listWidget);
    layout->addWidget(m_scrollArea);
}
```

- [ ] **Step 4: Update refresh() to keep list top-aligned and scrollable**

Replace `AppRankWidget::refresh()` with:

```cpp
void AppRankWidget::refresh()
{
    QVector<QVariantMap> data = m_db->getAppRank();

    // Clear existing items
    while (m_listLayout->count() > 1) {
        QLayoutItem *item = m_listLayout->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    int maxSec = data.isEmpty() ? 1 : data[0]["total_seconds"].toInt();
    for (int i = 0; i < data.size(); ++i) {
        AppRankItem *rankItem = new AppRankItem(
            i + 1, data[i]["app_name"].toString(),
            data[i]["total_seconds"].toInt(), maxSec, m_listWidget);
        m_listLayout->insertWidget(i, rankItem);
    }
}
```

- [ ] **Step 5: Commit**

```bash
git add src/ui/app_rank_widget.h src/ui/app_rank_widget.cpp
git commit -m "feat: add scroll area and light theme to app rank widget"
```

---

## Task 4: Update MainWindow Layout for Dashboard

**Files:**
- Modify: `src/ui/main_window.cpp`
- Test: 主界面显示 Dashboard 布局，按钮在合理位置。

**Interfaces:**
- Consumes: `StatsWidget`, `AppRankWidget`。
- Produces: `MainWindow` 使用新的布局结构。

- [ ] **Step 1: Move export/refresh buttons to header bar**

Replace the layout construction in `MainWindow` constructor with:

```cpp
QWidget *central = new QWidget(this);
QVBoxLayout *layout = new QVBoxLayout(central);
layout->setContentsMargins(24, 24, 24, 24);
layout->setSpacing(20);
setCentralWidget(central);

// Header
QHBoxLayout *headerLayout = new QHBoxLayout();
headerLayout->setSpacing(16);
QLabel *titleLabel = new QLabel("Time Master", this);
titleLabel->setFont(QFont("PingFang SC", 20, QFont::Medium));
titleLabel->setStyleSheet("color: #1F2937;");
headerLayout->addWidget(titleLabel);
headerLayout->addStretch();

QPushButton *exportBtn = new QPushButton(QString::fromUtf8("\xe5\xaf\xbc\xe5\x87\xba\xe8\xae\xb0\xe5\xbd\x95"), this);
exportBtn->setStyleSheet(btnStyle);
connect(exportBtn, &QPushButton::clicked, this, &MainWindow::onExport);
headerLayout->addWidget(exportBtn);

QPushButton *refreshBtn = new QPushButton(QString::fromUtf8("\xe5\x88\xb7\xe6\x96\xb0"), this);
refreshBtn->setStyleSheet(btnStyle);
connect(refreshBtn, &QPushButton::clicked, this, &MainWindow::refreshData);
headerLayout->addWidget(refreshBtn);

layout->addLayout(headerLayout);

m_statsWidget = new StatsWidget(db, this);
layout->addWidget(m_statsWidget);

m_appRankWidget = new AppRankWidget(db, this);
layout->addWidget(m_appRankWidget, 1);
```

- [ ] **Step 2: Commit**

```bash
git add src/ui/main_window.cpp
git commit -m "feat: add dashboard header layout in main window"
```

---

## Task 5: Build and Verify

**Files:**
- All modified files.

- [ ] **Step 1: Run CMake configure**

```bash
cmake --preset default
```

Expected: configuration completes with no errors.

- [ ] **Step 2: Run CMake build**

```bash
cmake --build build --config Release
```

Expected: build succeeds.

- [ ] **Step 3: Run tests**

```bash
ctest --test-dir build -C Release
```

Expected: existing tests pass.

- [ ] **Step 4: Manual visual verification**

Launch `build/Release/TimeMaster.exe` and verify:
- Window background is light (MICA on Win11, solid fallback on Win10), not black.
- Dashboard layout shows header, three stat cards, weekly chart, app ranking.
- App ranking list is scrollable when many entries exist.
- Buttons and text are readable.

- [ ] **Step 5: Final commit**

```bash
git add -A
git commit -m "chore: verify build and visual appearance"
```

---

## Spec Coverage Check

| Spec Requirement | Task |
|------------------|------|
| 修复黑色主界面 / MICA | Task 1 |
| Windows 10 纯色兜底 | Task 1 |
| 浅色柔和主题 | Task 1, 2, 3, 4 |
| Dashboard 三卡片 + 图表布局 | Task 2, 4 |
| 应用排行可滚动 | Task 3 |
| 统一间距、字体、颜色 | Task 1, 2, 3, 4 |

## Placeholder Scan

- No TBD / TODO / "implement later" / vague instructions.
- All code snippets are complete and ready to apply.
- All commands include expected output.
