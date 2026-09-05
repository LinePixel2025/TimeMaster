# AGENTS.md - Time Master

Windows 桌面时间追踪应用。项目使用 C++17、Qt 6 Widgets/Sql/Network/Svg、CMake 和 Ninja，当前版本为 5.6.3。

## 语言与环境

- 所有助手的自然语言回复、推理说明和新增代码注释使用简体中文。
- 代码标识符、Qt API、命令和现有英文注释保持英文。
- 终端为 PowerShell 7；命令示例使用 PowerShell 语法。
- 目标平台为 Windows 10/11，当前主力工具链是 Qt 6.11.1 MinGW 64 位：
  - Qt：`D:\AICOP\requirements\QT6\6.11.1\mingw_64`
  - MinGW：`D:\AICOP\requirements\QT6\Tools\mingw1310_64`
  - Ninja：`D:\AICOP\requirements\QT6\Tools\Ninja`
  - CMake：`D:\AICOP\requirements\QT6\Tools\CMake_64`
- `CMakePresets.json` 中的 `default` 是 Visual Studio 2022 + vcpkg 方案；MinGW 方案由 `build_mingw.ps1` 直接调用 Qt 的 `qt-cmake.bat` 配置。
- `README.md` 仍偏向 vcpkg/Visual Studio 工作流，执行构建时以本文件、`build_mingw.ps1` 和实际 CMake 配置为准。

## 常用命令

```powershell
# 推荐：清理 build，使用 Qt MinGW 配置并构建 Release 主程序（不构建测试）
.\build_mingw.ps1

# 运行主程序；程序默认常驻系统托盘
.\run.ps1
```

手动构建或运行测试时，先设置工具链路径：

```powershell
$env:PATH = "D:\AICOP\requirements\QT6\Tools\mingw1310_64\bin;D:\AICOP\requirements\QT6\6.11.1\mingw_64\bin;D:\AICOP\requirements\QT6\Tools\Ninja;D:\AICOP\requirements\QT6\Tools\CMake_64\bin;$env:PATH"
$env:QT_PLUGIN_PATH = "D:\AICOP\requirements\QT6\6.11.1\mingw_64\plugins"

# 配置测试构建；不要使用 build_mingw.ps1，因为它显式关闭 BUILD_TESTING
& "D:\AICOP\requirements\QT6\6.11.1\mingw_64\bin\qt-cmake.bat" -S . -B build-tests -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build-tests
ctest --test-dir build-tests --output-on-failure

# 也可以直接运行单个测试
.\build-tests\tests\test_database.exe
.\build-tests\tests\test_exporter.exe
.\build-tests\tests\test_lineweb_pusher.exe
.\build-tests\tests\test_cloud_sync.exe
.\build-tests\tests\test_ai_client.exe
.\build-tests\tests\test_reminder_scheduler.exe
.\build-tests\tests\test_weekly_report.exe
.\build-tests\tests\test_daily_report.exe
.\build-tests\tests\test_window_tracker.exe
.\build-tests\tests\test_trend_chart_layout.exe
.\build-tests\tests\test_trend_card_geometry.exe
.\build-tests\tests\test_dashboard_layout.exe
.\build-tests\tests\test_period_distribution_layout.exe
.\build-tests\tests\test_rank_layout.exe
```

测试目标是独立可执行文件，使用裸 `assert()` 和 Qt Test 的 `QSignalSpy`，没有 GoogleTest/Catch2。测试的 `main()` 必须创建 `QCoreApplication`（GUI/Widget 相关的几何与布局测试需创建 `QApplication` 或 `QGuiApplication`），否则 Qt SQL 插件或 GUI 模块可能在启动时崩溃。`test_window_tracker` 用 `FakeStore` 驱动 `TrackingEngine`，不访问真实数据库。涉及网络的 `test_lineweb_pusher` 会访问本机无效端口和测试地址，不要把它改成真实服务依赖；`test_cloud_sync` 在本地起 `QTcpServer`（`FakeHealthServer`）模拟健康 API，驱动 `LineWebPusher` 的推送/云端目标拉取，同样不依赖真实服务。`test_ai_client`、`test_reminder_scheduler`、`test_weekly_report` 只验证未配置/空数据的短路逻辑与无效端口的失败回退，不依赖真实 AI 服务。`test_weekly_report` 和 `test_daily_report` 通过注入临时目录，不写用户文档目录。UI 布局与几何测试（`test_trend_chart_layout`、`test_trend_card_geometry`、`test_dashboard_layout`、`test_period_distribution_layout`、`test_rank_layout`）纯本地计算，不依赖系统事件。

## 发布与安装包

- 主程序构建产物：`build\src\TimeMaster.exe`。
- 当前仓库没有 `package_installer.ps1`；不要在文档或脚本中假设该文件存在。
- `installer.iss` 使用 Inno Setup 6，从 `dist\TimeMaster` 复制文件并生成 `dist\TimeMaster-Setup-5.6.3.exe`。
- 发布前手动准备 `dist\TimeMaster`：复制主程序，执行 `windeployqt --no-translations --no-compiler-runtime --release`，再放入 MinGW 的 `libgcc_s_seh-1.dll`、`libstdc++-6.dll` 和 `libwinpthread-1.dll`。
- 安装目标默认为 `C:\Program Files\Time Master`；数据库路径来自 `QStandardPaths::AppDataLocation`，Windows 实际为 `%APPDATA%\TimeMaster\Time Master\data.db`（Roaming，非 LOCALAPPDATA），不应写入安装目录。
- 生成安装包需要 Inno Setup 6，默认编译器路径通常为 `C:\Users\22798\AppData\Local\Programs\Inno Setup 6\ISCC.exe`。
- 根目录当前可能存在未跟踪的 `Time Master.pro` 和 MinGW DLL。CMake 是正式构建入口，除非用户明确要求，不要擅自删除或纳入提交。

## 目录与架构

```text
src/
  main.cpp                  入口：数据库、主题、推送、追踪线程、主窗口和托盘的生命周期
  database/                 DatabaseManager：SQLite 会话、设置、忽略应用和应用别名
  tracker/                  WindowTracker：QThread 前台窗口轮询、空闲检测，线程内建独立数据库连接
                            TrackingEngine：会话状态机（Pending/Active/Idle），负责午夜切分、周期写库
                            TrackingStore：会话持久化接口，DatabaseManager 实现它
  icon/                     AppIconProvider：通过 Windows Shell 提取并缓存应用图标
  ui/                       MainWindow、主题管理器、托盘管理器和仪表盘卡片
                            HeroCard、TrendCard、RankCard、AiReportCard、SettingsDialog
                            CardFrame 容器与各种卡片纯布局辅助类（TrendChartLayout、DashboardLayout 等）
  ai/                       AiClient：OpenAI 兼容 Chat Completions 客户端，负责聚合统计、
                            构建中文 prompt、调用 AI 并缓存报告到 settings 表
  reminder/                 ReminderScheduler：按配置时间点定时触发托盘提醒，AI 短文案
                            优先、本地统计模板回退
                            WeeklyReportManager：每周固定时刻自动生成上一完整周 HTML 日报，
                            本地统计 + AI 分析（AI 未配置/失败回退本地小结）
  report/                 ReportHtmlBuilder：周报/日报共享的液态玻璃 HTML 渲染层
                            （CSS、SVG 折线图、热力图、时段条、应用排行等纯静态片段）
                            DailyReportManager：把「今日使用报告」生成为与周报同款的
                            HTML 网页（同日覆盖）；无定时调度，卡片「今日报告」查看时现场生成，
                            「生成分析」完成后经 applyReportText 自动更新
                            session_hours.h：会话按小时切开的共享算法，主页日晷与日/周报共用
  export/                   CSV 导出器和基于内置 miniz 的手写 XLSX 写入器
  push/                     LineWebPusher：定时/退出前向 LineWeb 推送当日总时长
  utility/                  Windows 自启动注册表辅助逻辑；ProcessIdentity 进程键归一化
third_party/miniz/          内置 miniz ZIP 实现，以 miniz_all.cpp 编译
resources/                  应用图标和 Qt resources.qrc
tests/                      test_database、test_exporter、test_lineweb_pusher、test_cloud_sync、test_ai_client、test_reminder_scheduler、test_weekly_report、test_daily_report、test_window_tracker、test_trend_chart_layout、test_trend_card_geometry、test_dashboard_layout、test_period_distribution_layout、test_rank_layout、test_theme_manager
docs/                       LineWeb 健康 API 文档（health-api.md）
docs/superpowers/           设计规格（仅本地保留，不入库）
.superpowers/sdd/           任务简报、审查差异和报告（仅本地，不入库）
```

根 CMakeLists 开启 `CMAKE_AUTOMOC` 和 `CMAKE_AUTORCC`，查找 Qt6 `Widgets Sql Svg Network Test`。主程序为 `WIN32` 子系统，不显示控制台，并链接 `dwmapi`、`shell32` 和 `gdi32`。

## 功能与数据约定

- 程序启动后默认隐藏到系统托盘；通过托盘菜单显示主窗口或退出。验证主窗口时需要临时调用 `window.show()`，验证结束后恢复。
- `WindowTracker` 轮询当前前台窗口，支持进程别名、忽略应用、追踪开关、轮询间隔、空闲阈值和最短追踪时长。轮询间隔使用数据库设置的 `poll_interval`，不能恢复成硬编码常量。4.0 起追踪逻辑抽为 `TrackingEngine` 状态机（Pending/Active/Idle），通过 `TrackingStore` 接口持久化，`WindowTracker` 只负责采集和线程管理。活跃会话按 `persistIntervalMs`（默认 30 秒）周期落库而非每秒写库，崩溃最多丢一个周期；时长以单调时钟为权威，墙钟偏差超过 5 秒时按"开始锚点 + 单调增量"推算（`sanitizeWallTime`），跨午夜会话自动切分且有段数上限（`kMaxMidnightSplits`）。5.6.2 起：标题读取用 `SendMessageTimeoutW(SMTO_ABORTIFHUNG, 150ms)`，挂起的前台进程不再卡死轮询线程；(hwnd, pid) 与上一轮相同时复用进程名/键/分类结果，不重复 OpenProcess 与文件 I/O；settle-in 防抖——同一前台需连续两个采样点（间隔 ≥ 一个轮询周期）才由 Pending 激活落库（激活门槛 `max(minTrackingMs, pollIntervalMs)`），存活不足一个周期的瞬时窗口静默丢弃、start 锚点仍取首个样本，因此 `min_tracking_seconds=0` 也不再产生 0~1 秒碎行；锁屏进程（`logonui.exe`/`winlogon.exe`，引擎内置 `kUnattendedProcessKeys`）截断会话进入 Idle 不计时；收尾写库失败时 `finishActive` 返回 false、本轮推迟切换/停用并下轮重试（正常退出路径 `stop()` 失败重试一次后接受丢失）；恰好跨午夜收尾不再产生 0 秒尾段。
- `MainWindow` 每 10 秒刷新数据，当前仪表盘固定包含 `HeroCard`（含昨日对比与四时段使用分布）、`TrendCard`、`RankCard` 和 `AiReportCard`。昨日对比已并入 Hero，不要重新引入 `CompareCard`。不要重新引入已删除的 `StatsWidget`、`AppRankWidget`、`DashboardCard`、`ReportDetailDialog`（每日报告详情弹窗）或旧的动态网格编辑器，除非任务明确要求。顶栏只保留主题、更多菜单（导出 / 云同步 / 刷新）和设置；状态芯片显示「追踪中 · 应用」/「空闲」/「已暂停」，由 `WindowTracker::activeWindowChanged` 队列连接更新（settle-in 防抖使新应用的「追踪中」显示最多延迟约一个轮询周期）。窄窗口（&lt; 1000px）改为单列。时段数据来自 `getAllSessions` + `SessionHours::addToDayHours`，禁止 `strftime('%H', start_time)` 分组。
- AI 智能模块：`AiClient` 通过 OpenAI 兼容 `POST {endpoint}/chat/completions` 生成报告（`Authorization: Bearer <key>`，解析 `choices[0].message.content`），支持 daily/weekly 两个周期。报告文本与锚点日期缓存于 `ai_report_{daily,weekly}_{text,date}` 设置，跨天/跨周后由 `AiReportCard` 判定过期。主页卡片点击「生成分析」→ `MainWindow::aiReportRequested` → `AiClient::generateReport`，结果经 `reportReady/reportFailed` 回填卡片并联动日报网页；`refreshData` 不触发 AI 请求。
- 每日报告网页：`DailyReportManager` 把今日统计（小时分布、时段、应用 Top5、会话洞察、目标完成度）+ AI 缓存文本生成为 `Documents/TimeMaster/日报-yyyy-MM-dd.html`（同日覆盖，纯 SVG/CSS 离线可用，样式与周报同款液态玻璃）。AI 未配置或无缓存时统计板块仍完整，AI 区显示引导空态。卡片「今日报告」经 `dailyReportOpenRequested` 由 main 调 `refreshToday()` 后 `openUrl` 打开；「生成分析」完成后经 `applyReportText` 自动更新并打开。报告 HTML 片段统一走 `src/report/report_html_builder`，周报与日报不得各自维护样式副本。小时切分与主页日晷共用 `src/report/session_hours.h`。
- 定时提醒：`ReminderScheduler` 用 30 秒 QTimer 轮询，`reminder_times` 为逗号分隔的 "HH:mm" 列表；到点以「日期|HH:mm」去重（同分钟一次、跨天自然失效），每次命中写 `reminder_last_fired` 触发记录（设置界面「上次触发」据此展示）。今日无数据时不发真实提醒，但弹一条说明"今日暂无使用记录"，避免配置了却不响。内容优先 `AiClient::generateReminderMessage`（`max_tokens: 200`，经 `reminderReady/reminderFailed` 按 tag 回填，不写缓存），AI 未配置或失败时回退 `buildLocalMessage` 本地统计模板（含今日时长、距目标差、主力应用）。托盘气泡由 `TrayManager::showNotification` 发出（底层 `Shell_NotifyIcon` 经典气泡；勿扰模式、系统通知被关闭等会导致不显示，非代码问题）。
- 间隔提醒：与时间点提醒并列的另一种模式（`reminder_interval_enabled`/`reminder_interval_minutes`，默认 45 分钟），自锚定时刻起每隔设定分钟数触发一次，触发逻辑（AI 优先/本地回退、无数据说明）复用 `fireReminderCore`。锚点 `m_intervalAnchor` 为内存态：首次检查仅锚定不触发，程序重启后重新计时；`reloadSettings` 仅在间隔配置变化时清空锚点重新计时，无关设置的 reload 不得误重置。测试经 `checkNow(QDateTime)` 注入时刻驱动。
- `ReminderScheduler` 与 `WeeklyReportManager` 的 `start()` 必须调用 `m_timer->start()` 启动 30 秒轮询（曾漏掉导致只在启动瞬间检查一次）；`isRunning()` 为防回归断言，测试中有覆盖。
- 每周周报：`WeeklyReportManager` 每周固定周几 + 时刻（`weekly_report_day`/`weekly_report_time`）自动生成上一完整周的 HTML 日报，输出到 `Documents/TimeMaster/周报-yyyy-MM-dd.html`（测试用 `setOutputDir` 注入）。统计部分由本地数据组装（总览、每日时长、应用 Top5、环比），AI 已配置且该周有数据时经 `AiClient::generateWeekReport`（`buildPromptForRange`，不写缓存）异步回填「AI 分析」区，失败回退 `buildLocalSummary`。去重键为上周一日期（`weekly_report_last_generated`），同周只生成一次。主页「上周周报」按钮经 `weeklyReportOpenRequested` 由 main 用 `QDesktopServices::openUrl` 打开。
- `ThemeManager` 是主题单例，主题值为 `light`/`dark`，通过 `themeChanged` 通知卡片刷新，并更新应用调色板和 Windows 标题栏。颜色集中在 `ui/design_tokens.h`；新增 UI 优先使用设计 token。
- 主题色（accent）可在设置「个性化」页自定义：预设色块 + `QColorDialog` 取色，点击即时预览（`setAccentColor(color, persist=false)`），保存时 `commitAccent()` 落库（设置键 `accent_color`，`#RRGGBB` 或哨兵值 `default` = 内置默认玉色，该表 value 列 NOT NULL、不可存空串），`reject()` 负责取消还原；`ThemeManager::accentChanged` 通知各卡片重绘。`design_tokens.h` 的 accent 系列 token 采用双路径：未自定义时返回内置硬编码色（默认视觉零变化），自定义后由 brand 色按 HSL 派生（`deepAccent` 压暗 ≤0.45 / `brightAccent` 提亮 ≥0.55）；报告 HTML 的 accent/热力/时段色阶由 `report_html_builder` 经 `accentFor/accentLightFor/heatLevelFor` 显式亮暗两套派生填充，不要在报告中恢复硬编码 accent。
- 设置界面管理以下设置：`tracking_enabled`、`poll_interval`、`idle_threshold`、`min_tracking_seconds`、`min_record_threshold`、`auto_start`、`theme`、`accent_color`、`daily_goal`,`lineweb_enabled`、`lineweb_endpoint`、`lineweb_token`、`lineweb_interval`、`lineweb_last_push`、`lineweb_last_fetch`、`lineweb_pending_push`、`ai_enabled`、`ai_api_endpoint`、`ai_api_key`、`ai_model`、`ai_report_daily_text`、`ai_report_daily_date`、`ai_report_weekly_text`、`ai_report_weekly_date`、`reminder_enabled`、`reminder_times`、`reminder_last_fired`、`reminder_interval_enabled`、`reminder_interval_minutes`、`weekly_report_enabled`、`weekly_report_day`、`weekly_report_time`、`weekly_report_last_generated`、`weekly_report_path`、`trend_display_format`、`trend_heatmap_period`。新增设置沿用 `settings` 表的 key/value 形式并提供默认值。
- 数据库构造时自动创建/迁移 `sessions`、`settings`、`ignored_apps`、`app_aliases` 表，迁移使用 `CREATE ... IF NOT EXISTS`，没有版本号迁移框架。
- 统计查询会应用最短记录阈值 `min_record_threshold` 和忽略应用过滤；修改查询时要保持这两个行为一致。
- 按日统计一律用 `start_time >= ? AND start_time < 次日` 的定长 ISO 文本开区间（辅助 `dayEndExclusive`），分组键用 `substr(start_time,1,10)`；禁止改回 `date(start_time)` 包裹写法——函数会使 `idx_sessions_start` 失效，退化为全表扫描。
- SQLite 连接默认启用 WAL + `synchronous=NORMAL`（`DatabaseManager` 构造时设置），排障与备份时不应假设回滚日志模式；用户数据库目录会因此出现 `data.db-wal`/`data.db-shm` 伴生文件，属正常现象。
- LineWeb 推送请求发送到 `<endpoint>/api/health/push`，JSON 字段为 `totalSeconds`、`date`，认证头为 `X-Screen-Time-Token`。5.0 起 `LineWebPusher` 支持云端同步：主页「云端同步」按钮（`MainWindow::cloudSyncRequested` → `LineWebPusher::syncNow`）立即补推并拉取 `GET <endpoint>/api/health/daily-goal/data` 的云端每日目标（响应字段 `goal`，写回 `daily_goal` 设置），拉取时间记入 `lineweb_last_fetch`；失败/未配置时弹 `QMessageBox` 提示，不影响其他功能。云端协议细节以 `docs/health-api.md` 为准。设置变更后必须让 `LineWebPusher`、`WindowTracker` 和 `AiClient` 重新加载配置。

## 数据库线程安全

4.0 起追踪线程在 `run()` 内用 `DatabaseManager threadDb(m_db->databasePath())` 创建**独立 SQLite 连接**，不再与 GUI 线程共享 `m_db`；GUI 线程（设置、统计、主窗口、托盘）只访问自己的连接。`WindowTracker` 的运行时配置（`TrackingConfig` 快照、别名、`m_configRevision`）由 `m_settingsMutex` 保护。

即便如此，`DatabaseManager` 的所有公开方法仍必须先获取 `QMutexLocker lock(&m_mutex)`，包括读取设置和统计的方法；不要只给写操作加锁。这是防双连接/多线程复用的兜底约定，也保持与 `TrackingStore` 接口一致。

```cpp
int DatabaseManager::getTodayTotal()
{
    QMutexLocker lock(&m_mutex);
    QSqlQuery q(m_db);
    // ...
}
```

关闭顺序必须保持：停止 LineWeb 定时器并尝试最终推送，停止并等待 `WindowTracker`，然后调用 `DatabaseManager::close()`，最后退出 Qt 事件循环。`close()` 具有 `m_closed` 防重入保护，不要绕过它直接移除数据库连接。

## 导出约定

- CSV 和 XLSX 导出逻辑位于 `src/export`，XLSX 是 Office Open XML + miniz ZIP 的本地实现，不要无必要引入新库。
- `XlsxWriter::buildSheet()` 会延迟收集共享字符串索引，因此保存 XLSX 时必须先生成所有 sheet，再生成 `sharedStrings.xml`。若调整 `XlsxWriter::save()`，保持这个顺序，否则共享字符串计数可能为 0，Excel 内容会损坏。
- 修改导出格式后至少运行 `test_exporter`，并检查生成的 XLSX 能被 ZIP/XML 工具读取。

## UI 约定

- Windows 字体使用 `Microsoft YaHei`，不要使用 `PingFang SC`。优先复用 `DesignTokens::appFont()` 和 `DesignTokens` 中的颜色、间距、圆角。数字/时长使用 `DesignTokens::monoFont()`，主程序时长格式为 `3h 10m`。
- `ThemeManager::applyToApplication` 必须读 `DesignTokens`，禁止再写死靛蓝调色板，否则原生对话框会与仪表盘串色。
- 保持纯色窗口背景和现有主题体系。禁止重新引入 `WA_TranslucentBackground`、`DwmSetWindowAttribute(DWMWA_SYSTEMBACKDROP_TYPE)` 或 `DwmExtendFrameIntoClientArea`；这些方案曾导致不同 Windows 版本出现黑色客户区。
- 修改 UI 后先构建，再通过托盘显示窗口进行手动检查；检查浅色/深色主题、窄窗口、长中文文本、空数据和托盘退出流程。
- 不要为了验证而提交临时的 `window.show()`、截图或运行时数据库。

## 已知陷阱

| 陷阱 | 症状 | 处理 |
|------|------|------|
| 数据库读取缺少 `QMutexLocker` | `qsqlite.dll` 中出现 `0xc0000005` | 所有 `m_db` 方法加锁 |
| 测试 `main()` 缺少 `QCoreApplication` | Qt6Sql 启动时崩溃 | 在测试入口创建应用对象 |
| 运行测试没有 `QT_PLUGIN_PATH` | 找不到 QSQLITE 驱动或 `0xc0000135` | 设置 Qt plugins 路径和完整 MinGW/Qt PATH |
| 运行中的 `TimeMaster.exe` 占用输出文件 | linker permission denied | 构建前退出程序；必要时使用 `taskkill /f /im TimeMaster.exe` |
| `WindowTracker` 使用硬编码轮询间隔 | 用户设置不生效 | 使用 `TrackingConfig::pollIntervalMs`，并在 reload 时读取 `poll_interval` |
| 统计查询用 `date(start_time)` 包裹 | `idx_sessions_start` 失效，仪表盘刷新全表扫描变慢 | 改用定长 ISO 文本开区间 `start_time >= ? AND < 次日`（`dayEndExclusive`） |
| 追踪线程共享 GUI 线程的 `QSqlDatabase` | 跨线程 SQLite 竞态/崩溃 | 在 `run()` 内用 `m_db->databasePath()` 创建独立 `DatabaseManager` |
| `DatabaseManager::close()` 重复调用 | 移除无效连接或退出异常 | 保留并使用 `m_closed` 防重入 |
| 忽略应用重复添加 | 返回 0 或重复项 | 先查询已有 ID，再插入 |
| XLSX 先生成 `sharedStrings.xml` | Excel 中文/文本显示异常 | 先调用全部 `buildSheet()`，再调用 `buildSharedStrings()` |
| 使用旧 UI 类名或旧布局文档 | 编译找不到头文件/源文件 | 以当前 `src/ui` 和 `src/CMakeLists.txt` 为准 |

## SDD 与 Git

- 处理较大功能前可先查看本地（未入库）的 `.superpowers/sdd/` 和 `docs/superpowers/` 中相关规格；规格可能描述历史实现，若与当前代码冲突，以当前代码和用户任务为准。
- 不要回滚或覆盖用户已有的未提交修改。开始编辑前检查 `git status`，完成后再次检查只包含任务相关变更。
- 提交使用约定式提交，例如 `feat:`、`fix:`、`refactor:`、`chore:`。除非用户要求，不要自动创建提交、分支或修改发布产物。
- 项目目前没有 lint、格式化、静态类型检查或 CI/CD 流水线；验证以构建、CTest、独立测试和必要的手动 UI 检查为主。
