# Time Master 前端 BUG 修复与 UI 重设计

Date: 2026-07-13

## 问题

1. 主界面显示为黑色，没有 MICA 效果。
2. 应用使用排行列表没有滚动，应用条目多时会挤在一起。
3. 整体界面视觉简陋，需要重设计。

## 根因分析

- `MainWindow` 同时设置了 `Qt::WA_TranslucentBackground` 和 `setAutoFillBackground(false)`，且没有兜底背景色。当 DWM 的 `DWMWA_SYSTEMBACKDROP_TYPE` 属性未生效或系统不支持时，窗口背景直接显示为黑色。
- `AppRankWidget` 内部使用普通 `QWidget` 容纳列表项，缺少 `QScrollArea`，布局会无限增长。
- 现有 UI 硬编码字体、颜色、间距，缺乏统一的视觉层次和留白。

## 设计方向

- 浅色柔和 MICA（Light Soft MICA）
- Dashboard 式布局

## 色彩系统

| 名称 | 色值 | 用途 |
|------|------|------|
| `--bg-fallback` | `#F4F6F8` | Windows 10 / DWM 失效时的兜底背景 |
| `--card-bg` | `rgba(255,255,255,0.72)` | 玻璃卡片背景 |
| `--card-border` | `rgba(255,255,255,0.6)` | 卡片边框 |
| `--text-primary` | `#1F2937` | 主标题、数字 |
| `--text-secondary` | `#6B7280` | 副标题、说明文字 |
| `--accent` | `#6366F1` | 按钮、进度条、图表 |
| `--accent-light` | `#818CF8` | 悬停、高亮 |

## 布局

```
┌──────────────────────────────────────────────────┐
│  Time Master                      [导出] [刷新]  │
├──────────────────────────────────────────────────┤
│  ┌──────────┐  ┌──────────┐  ┌──────────┐       │
│  │ 今日总时长 │  本周总时长  │ 最常用应用 │       │
│  │  环形进度  │    大数字   │  名称+时长  │       │
│  └──────────┘  └──────────┘  └──────────┘       │
│  ┌──────────────────────────────────────────┐  │
│  │ 本周趋势（横向柱状图）                     │  │
│  └──────────────────────────────────────────┘  │
│  ┌──────────────────────────────────────────┐  │
│  │ 应用使用排行（可滚动列表）                 │  │
│  │ 1. 应用名  ████████  1h 30m              │  │
│  │ 2. 应用名  █████      45m               │  │
│  └──────────────────────────────────────────┘  │
└──────────────────────────────────────────────────┘
```

## 组件调整

### MainWindow

- 移除 `Qt::WA_TranslucentBackground`。
- 按 Windows 版本选择背景策略：
  - Windows 11（build >= 22000）：启用 `DWMWA_SYSTEMBACKDROP_TYPE` 的 MICA 效果，窗口背景透明以展示 MICA。
  - Windows 10 或 DWM 不可用：使用纯色兜底背景 `#F4F6F8`。
- 修复 `DWMWA_USE_IMMERSIVE_DARK_MODE` 常量类型，使用 `DWORD`。

### StatsWidget

- 从两卡片布局改为 Dashboard 三卡片 + 图表布局。
- 使用 `GlassCard` 作为卡片容器，支持浅色主题。
- 新增 `TodayTotalCard`、`WeekTotalCard`、`TopAppCard`（可用 QLabel 或简单 QWidget 实现）。
- 保留并美化 `CircularProgress` 和 `WeeklyBar`。

### AppRankWidget

- 列表容器放入 `QScrollArea`。
- 设置 `setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff)`。
- 列表项保持固定高度 48px，增加间距和圆角。
- 刷新时清空列表并重新创建 `AppRankItem`。

### AppRankItem

- 保持自定义绘制。
- 调整字体颜色为 `text-primary` / `text-secondary`。
- 进度条使用 `#6366F1` -> `#818CF8` 渐变。

### 字体

- 继续使用 `PingFang SC`，但统一通过 `QFont` 或样式表设置字号层次：
  - 标题 16px / Medium
  - 数字 28-32px / Light
  - 正文 13px / Normal
  - 辅助 11px / Normal

## 修复清单

- [ ] 修复 MICA 黑色背景问题。
- [ ] 为应用排行添加滚动区域。
- [ ] 重设计 Dashboard 布局与浅色主题。
- [ ] 统一间距、字体、颜色。
- [ ] 构建验证（如环境可用）。

## 兼容性

- Windows 11：MICA 效果正常。
- Windows 10：回退到纯色浅色背景，无 MICA 但不再黑屏。
- 保持 Qt6 Widgets，不引入新依赖。
