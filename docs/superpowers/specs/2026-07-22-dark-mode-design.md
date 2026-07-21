# Dark Mode — 设计规格

**日期:** 2026-07-22
**状态:** 待审核

---

## 概述

为 Time Master 添加黑暗模式，主窗口顶栏放置切换按钮，支持明亮/黑暗一键切换。切换自动持久化到数据库，下次启动恢复。

---

## 用户故事

1. 用户在主页点击 ☀/🌙 按钮，整体界面即时切换为暗色主题
2. 关闭应用后重新打开，主题保持上次选择
3. 暗色模式下所有内容（卡片、图表、文字、按钮、设置对话框）清晰可见

---

## 架构

### 新增：ThemeManager 单例 (`src/ui/theme_manager.h/cpp`)

```
ThemeManager : QObject
├── enum Theme { Light, Dark }
├── instance()           → ThemeManager*
├── currentTheme()       → Theme
├── isDark()             → bool
├── toggle()             → 切换 Light↔Dark
├── setTheme(Theme)      → 设置指定主题并应用
├── signal themeChanged(Theme)
├── applyToApplication() → 设置全局 QPalette + 基础 QSS
├── loadFromDb()         → 从 settings 表读 'theme' key
└── saveToDb(Theme)      → 写入 settings 表
```

### 改造：DesignTokens (`src/ui/design_tokens.h`)

所有 `inline const QColor` 常量改为 `inline QColor` 函数，内部调用 `ThemeManager::instance()->isDark()` 返回对应色值。

亮色值保持不变，新增暗色值。追加以下当前缺失但被硬编码使用的 token：

| 新增 Token | 用途 | Light | Dark |
|------------|------|-------|------|
| `kCardBorder` | 卡片描边 | `rgba(0,0,0,20)` | `rgba(255,255,255,10)` |
| `kButtonHoverBg` | 按钮 hover 背景 | `#E5E7EB` | `rgba(255,255,255,0.08)` |
| `kChartGradientTop` | 图表渐变顶部 | `#A5B4FC` | `#6366F1` |
| `kChartGradientBottom` | 图表渐变底部 | `#6366F1` | `#4338CA` |
| `kChartAreaTop` | 折线图面积渐变顶 | `rgba(165,180,252,60)` | `rgba(99,102,241,40)` |
| `kChartAreaBottom` | 折线图面积渐变底 | `rgba(165,180,252,0)` | `rgba(99,102,241,0)` |
| `kTodayGlow` | 今日发光高亮 | `rgba(99,102,241,38)` | `rgba(129,140,248,45)` |
| `kTodayDotBg` | 今日大圆点底色 | `#FFFFFF` | `#2D2D3F` |
| `kProgressBg` | 进度条背景 | `#E5E7EB` | `rgba(255,255,255,0.12)` |
| `kCompareTodayBg` | 对比卡片今日背景 | `rgba(99,102,241,20)` | `rgba(129,140,248,25)` |
| `kCompareYesterdayBg` | 对比卡片昨日背景 | `rgba(0,0,0,4)` | `rgba(255,255,255,2)` |
| `kCompareYesterdayBar` | 对比昨日条 | `#D1D5DB` | `#475569` |
| `kPlaceholderIcon` | 占位图标灰 | `#9CA3AF` | `#64748B` |
| `kPlaceholderBg` | 占位图标背景 | `#E5E7EB` | `#334155` |
| `kSeparator` | 分隔线 | `rgba(0,0,0,10)` | `rgba(255,255,255,6)` |
| `kTabInactiveBg` | Tab 未选中背景 | `transparent` | `transparent` |
| `kTabInactiveText` | Tab 未选中文字 | `#4B5563` | `#94A3B8` |

---

## 颜色映射表（完整 Dark 色值）

### 核心 Token

| Token | Light | Dark |
|-------|-------|------|
| `kBg` | `#F8F9FB` | `#1E1E2E` |
| `kSurface` | `#FFFFFF` | `#2D2D3F` |
| `kBorder` | `rgba(0,0,0,8)` | `rgba(255,255,255,8)` |
| `kAccent` | `#6366F1` | `#818CF8` |
| `kAccentLight` | `#A5B4FC` | `#6366F1` |
| `kAccentHover` | `#4F46E5` | `#A5B4FC` |
| `kAccentPressed` | `#4338CA` | `#C7D2FE` |
| `kAccentGlow` | `rgba(99,102,241,20)` | `rgba(129,140,248,25)` |
| `kSuccess` | `#10B981` | `#34D399` |
| `kError` | `#EF4444` | `#F87171` |
| `kTextStrong` | `#111827` | `#F1F5F9` |
| `kText` | `#374151` | `#CBD5E1` |
| `kTextMute` | `#6B7280` | `#94A3B8` |
| `kTextFaint` | `#9CA3AF` | `#64748B` |

### 壁纸 Token

| Token | Light | Dark |
|-------|-------|------|
| `kWhiteBg` | `#FFFFFF` | `#2D2D3F` |
| `kFrostStart` | `#EFF2F9` | `#252540` |
| `kFrostEnd` | `#F5F0EC` | `#2A2535` |

### 新增 Token（见上表）

---

## 全局 QPalette

`ThemeManager::applyToApplication()` 设置 `qApp->palette()`：

| 角色 | Light | Dark |
|------|-------|------|
| `Window` | `#F0F2F5` | `#1E1E2E` |
| `Base` | `#FFFFFF` | `#2D2D3F` |
| `AlternateBase` | `#F8F9FB` | `#252538` |
| `Text` | `#1F2937` | `#F1F5F9` |
| `WindowText` | `#1F2937` | `#F1F5F9` |
| `Button` | `#FFFFFF` | `#2D2D3F` |
| `ButtonText` | `#1F2937` | `#F1F5F9` |
| `Highlight` | `#6366F1` | `#818CF8` |
| `HighlightedText` | `#FFFFFF` | `#1E1E2E` |
| `ToolTipBase` | `#FFFFFF` | `#2D2D3F` |
| `ToolTipText` | `#1F2937` | `#F1F5F9` |

---

## 切换按钮

- **位置：** 主窗口顶栏，Title 和设置按钮之间
- **图标：** 纯文本 Unicode — 亮色模式显示 `🌙`，暗色模式显示 `☀`
- **样式：** 与设置按钮 `⚙` 一致（透明背景、灰色文字、hover 浅底）
- **交互：** 点击调用 `ThemeManager::toggle()`，连接 `themeChanged` 更新自身文字

---

## 数据库持久化

- **表：** 复用 `settings` 表
- **Key：** `theme`
- **Value：** `light` / `dark`
- **读：** `main.cpp` 启动时，`DatabaseManager` 初始化后，调用 `ThemeManager::instance()->loadFromDb(m_db)`
- **写：** 每次切换主题时 `ThemeManager` 内部调用 `m_db->setSetting("theme", ...)`

（`getSetting`/`setSetting` 已存在于 `DatabaseManager`，无需新增。）

---

## 改造影响范围

### 场景分类

| 类型 | 处理方式 | 涉及文件 |
|------|----------|----------|
| QSS 样式 | `themeChanged` 信号槽中重建 `setStyleSheet()` | main_window, dashboard_card, hero_card, topapp_card, insight_card, settings_dialog |
| paintEvent 自绘 | `themeChanged` → `update()`，`paintEvent` 内从 token 函数读色 | chart_card, rank_card, compare_card, dashboard_card, grid_editor, stats_widget, app_rank_widget |
| 原生控件 | `applyToApplication()` 设置 QPalette，自动继承 | 全部 `QScrollArea`、`QLineEdit`、`QComboBox` 等 |

### 文件改动清单

| 文件 | 改动类型 | 说明 |
|------|----------|------|
| `src/ui/theme_manager.h` | **新增** | ThemeManager 声明 |
| `src/ui/theme_manager.cpp` | **新增** | ThemeManager 实现 |
| `src/ui/design_tokens.h` | 重构 | 常量→函数，加暗色值，加新 token |
| `src/main.cpp` | 修改 | 启动时调用 `loadFromDb()` |
| `src/ui/main_window.cpp` | 修改 | 加 toggle 按钮，QSS 转 token，连信号 |
| `src/ui/dashboard_card.cpp` | 修改 | QSS + paintEvent 转 token，连信号 |
| `src/ui/hero_card.cpp` | 修改 | 连 `themeChanged` → rebuild stylesheet |
| `src/ui/chart_card.cpp` | 修改 | 硬编码 QColor → token 函数，连信号 |
| `src/ui/rank_card.cpp` | 修改 | 同上 |
| `src/ui/compare_card.cpp` | 修改 | 同上 |
| `src/ui/topapp_card.cpp` | 修改 | 连 `themeChanged` → rebuild stylesheet |
| `src/ui/insight_card.cpp` | 修改 | 混合硬编码 → token，连信号 |
| `src/ui/settings_dialog.cpp` | 修改 | 大量 QSS → token |
| `src/ui/grid_editor.cpp` | 修改 | paintEvent → token |
| `src/ui/stats_widget.cpp` | 修改 | 旧组件，全硬编码 → token |
| `src/ui/app_rank_widget.cpp` | 修改 | 旧组件，全硬编码 → token |
| `src/ui/app_icon_provider.cpp` | 修改 | 占位图标色 → token |
| `src/CMakeLists.txt` | 修改 | 添加 `theme_manager.cpp` |

---

## 启动流程

```
main.cpp:
  1. QApplication 创建
  2. DatabaseManager 初始化（含数据库迁移）
  3. ThemeManager::instance()->loadFromDb(m_db)  ← 读 'theme' setting
  4. ThemeManager::applyToApplication()           ← 设置全局 QPalette
  5. MainWindow 创建（窗口诞生即处于正确主题）
  6. WindowTracker / TrayManager 启动
  7. QApplication::exec()
```

---

## 不变项

- 图表数据计算逻辑不变
- Dashboard 布局 JSON 格式不变
- 数据库表结构不变（复用 settings 表）
- 跟踪线程、CSV/XLSX 导出、系统托盘逻辑不变
- `wallpaper_helper` 壁纸生成不变（wallpaper token 适配后即可）
- 字体方案不变

---

## 测试要点

1. **启动默认亮色** — 首次运行（DB 无 theme 记录）显示亮色
2. **切换** — 点击按钮即时切换到暗色/亮色，所有卡片可见
3. **持久化** — 切换到暗色，关闭重启后仍为暗色
4. **图表** — 柱状图、折线图在暗色背景下清晰可读
5. **设置对话框** — 打开设置，暗色模式下所有页签、控件可见
6. **网格编辑器** — 拖拽编辑布局时暗色下正常显示
7. **旧组件** — stats_widget / app_rank_widget 暗色下正常（虽然主窗口未使用）
