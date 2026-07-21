# Task 4 Report: Refactor StatsWidget Layout with Chart Toggle

## 状态：完成

## 提交
- **SHA**: `25e6861`
- **消息**: `feat: redesign weekly trend with left-right split layout and chart toggle`

## 构建结果
构建成功 — 所有 5 个步骤（MOC + 4 个编译单元 + 链接）无错误。

## 变更摘要

### stats_widget.h
- 新增 `#include <QStackedWidget>`
- 私有成员新增：`m_weeklyLine`、`m_chartStack`、`m_yesterdayCompare`

### stats_widget.cpp
- 新增 includes：`<QStackedWidget>`、`<QPushButton>`、`<QButtonGroup>`
- 构造函数：将单独的周视图卡片替换为两个并排卡片：
  - 左侧（stretch 3）："每日趋势" GlassCard，带图表切换按钮（柱状图/折线图）、QStackedWidget（WeeklyBar + WeeklyLine）、持久化切换偏好（`getSetting`/`setSetting`）
  - 右侧（stretch 2）："较昨日" GlassCard，包含 YesterdayCompare
- `refresh()`：同时更新 `m_weeklyBar` 和 `m_weeklyLine` 数据，调用 `m_yesterdayCompare->setData()` 并传入今日和昨日总计

## 关注点
无。

## 报告路径
`D:\AICOP\Projects\Time Master\.superpowers\sdd\task-4-report.md`
