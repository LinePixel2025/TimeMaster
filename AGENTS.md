# AGENTS.md — Time Master

Windows 桌面时间追踪应用。C++17、Qt6 Widgets+Sql、CMake + Ninja。

## 环境要求

- **Qt6 SDK** 安装于 `D:\AICOP\requirements\QT6`（6.11.1，MinGW 64 位）
  - MinGW 13.1.0 位于 `Tools\mingw1310_64`
  - Ninja 位于 `Tools\Ninja`
  - CMake 位于 `Tools\CMake_64`
- **备选方案（vcpkg）：** `CMakePresets.json` 也提供了 `default` preset（Visual Studio 2022 + vcpkg），但主力工作流为 MinGW。

## 常用命令

```powershell
# 一键构建（脚本）
.\build_mingw.ps1

# 运行
.\run.ps1

# ---- 或手动执行 ----

# 设置 MinGW 的 PATH（任何构建/运行命令之前必须执行）
$env:PATH = "D:\AICOP\requirements\QT6\Tools\mingw1310_64\bin;D:\AICOP\requirements\QT6\6.11.1\mingw_64\bin;D:\AICOP\requirements\QT6\Tools\Ninja;D:\AICOP\requirements\QT6\Tools\CMake_64\bin;$env:PATH"

# 配置
cmake --preset mingw

# 构建
cmake --build build

# 运行测试（需要 QT_PLUGIN_PATH 加载 SQL 驱动）
$env:QT_PLUGIN_PATH = "D:\AICOP\requirements\QT6\6.11.1\mingw_64\plugins"
.\build\tests\test_database.exe
.\build\tests\test_exporter.exe

# 打包发布
$env:PATH = "D:\AICOP\requirements\QT6\6.11.1\mingw_64\bin;D:\AICOP\requirements\QT6\Tools\mingw1310_64\bin;$env:PATH"
Remove-Item -Recurse -Force dist\TimeMaster -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Path dist\TimeMaster -Force | Out-Null
Copy-Item build\src\TimeMaster.exe dist\TimeMaster\
& windeployqt --no-translations --no-compiler-runtime --release dist\TimeMaster\TimeMaster.exe
Copy-Item D:\AICOP\requirements\QT6\Tools\mingw1310_64\bin\libgcc_s_seh-1.dll dist\TimeMaster\
Copy-Item D:\AICOP\requirements\QT6\Tools\mingw1310_64\bin\libstdc++-6.dll dist\TimeMaster\
Copy-Item D:\AICOP\requirements\QT6\Tools\mingw1310_64\bin\libwinpthread-1.dll dist\TimeMaster\

# 一键构建 + 打包 + 生成安装包（.exe）
.\package_installer.ps1

# 或仅生成安装包（需先构建并打包好 dist\TimeMaster）
& "C:\Users\22798\AppData\Local\Programs\Inno Setup 6\ISCC.exe" installer.iss
```

## 安装包

通过 Inno Setup 6 生成单文件安装包，用户在其他电脑上可直接运行安装。

- **安装脚本：** `installer.iss`（Inno Setup 配置）
- **一键脚本：** `package_installer.ps1`（构建 → 打包 → 安装包）
- **安装包依赖：** Inno Setup 6（`winget install JRSoftware.InnoSetup`）
- **安装路径：** `C:\Program Files\Time Master`
- **数据库位置：** `%LOCALAPPDATA%\TimeMaster\data.db`（AppData，无需管理员权限）
- **输出：** `dist\TimeMaster-Setup-<version>.exe`

```

项目无 lint、格式化或类型检查命令，无 CI/CD 流水线。

## 架构

```
src/
  main.cpp            — 入口（连接数据库、追踪器、主窗口、托盘）
  database/           — SQLite 会话存储（DatabaseManager，构造时自动迁移）
  tracker/            — 前台窗口轮询线程（WindowTracker : QThread）
  icon/               — 应用图标提取与缓存（AppIconProvider，通过 SHGetFileInfoW 提取 EXE 图标）
  ui/                 — MainWindow（纯色浅色背景）、StatsWidget、AppRankWidget、TrayManager
  export/             — CSV 导出器 + 手写 XLSX 写入器（基于内置 miniz）
  CMakeLists.txt      — 添加 src/include 目录，链接 Qt6::Widgets Qt6::Sql dwmapi shell32 gdi32
third_party/miniz/    — 内置 miniz（zlib 兼容，C 源码封装为 .cpp）
resources/            — 图标 + resources.qrc
tests/                — 两个测试可执行文件，基于原始 assert()，无测试框架
dist/                 — 预构建的 Time Master.exe + 所有 DLL（独立运行）
.superpowers/sdd/     — SDD 任务跟踪（简报、报告、审查差异）
```

## 核心约定

- **包含路径：** `src/` 下的头文件作为私有包含添加，因此使用 `database/database_manager.h`（不加 `src/` 前缀）。`third_party/miniz/` 在 miniz cpp 封装中使用相对路径。
- **Qt6 AUTOMOC/AUTORCC：** 根 CMakeLists 中两者均为 ON。`Q_OBJECT` 类和 `.qrc` 文件自动工作。
- **测试：** 在 `tests/test_*.cpp` 中使用裸 `assert()`。每个测试是独立的可执行文件；生产源码直接编译进测试目标（无共享库）。临时数据库使用 `QTemporaryFile`。返回 0 表示通过。**测试的 `main()` 必须包含 `<QCoreApplication>`** — SQL 插件加载需要它。
- **SQLite** 通过 Qt6 的 SQL 模块内嵌使用 — 无需独立库。
- **XLSX：** 手写 `XlsxWriter` 类，无第三方库。将 Office Open XML 写入 miniz ZIP 流。**关键约束：`buildSharedStrings()` 必须在所有 `buildSheet()` 之后调用**，因为 `buildSheet()` 会延迟追加共享字符串索引。顺序错误会导致 `sharedStrings.xml` 中 `count="0"`，Excel 无法正确显示内容。
- **数据库迁移：** `DatabaseManager::migrate()` 在构造时运行，使用 `IF NOT EXISTS`（无版本化迁移框架）。
- **仅限 Windows：** 链接 `dwmapi` 用于标题栏主题。主 exe 使用 `WIN32` 隐藏控制台窗口。
- **背景：** MICA 背景已移除。始终使用纯色 `#F0F2F5` 背景。**禁止**重新引入 `WA_TranslucentBackground`、`DwmSetWindowAttribute(DWMWA_SYSTEMBACKDROP_TYPE)`、`DwmExtendFrameIntoClientArea` — 它们在不同 Windows 版本上不稳定。
- **构建产物：** `build/` 已在 gitignore 中，`.db` 文件和 `dist/` 同理。
- **提交风格：** 约定式提交（`feat:`、`fix:`、`chore:`）。

## 数据库线程安全

**所有访问 `m_db` 的方法必须获取 `QMutexLocker lock(&m_mutex)`。** 追踪线程调用写入方法（`insertSession`、`updateSessionDuration`、`updateSessionEnd`），同时 UI 线程通过 10 秒 QTimer 调用读取方法（`getTodayTotal`、`getWeekSummary`、`getAppRank` 等）。读取方法不加锁会导致并发访问时在 `qsqlite.dll` 中产生 `0xc0000005`（访问冲突）崩溃。

```cpp
// 正确 — 所有方法遵循此模式：
int DatabaseManager::getTodayTotal()
{
    QMutexLocker lock(&m_mutex);   // <-- 必须
    QSqlQuery q(m_db);
    // ...
}
```

## 字体约定

在 Windows 上使用 `Microsoft YaHei`（不要用 `PingFang SC`）。每个 UI 文件定义本地辅助函数：

```cpp
static QFont appFont(int size, QFont::Weight weight = QFont::Normal)
{
    QFont font("Microsoft YaHei", size, weight);
    font.setStyleStrategy(QFont::PreferAntialias);
    return font;
}
```

## UI 验证工作流

本模型无法直接查看图像。验证 UI 更改的步骤：

```powershell
# 1. 构建并启动应用（临时在 main.cpp 中添加 window.show()）
# 2. 截取全屏截图：
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
$screen = [System.Windows.Forms.Screen]::PrimaryScreen
$bitmap = New-Object System.Drawing.Bitmap($screen.Bounds.Width, $screen.Bounds.Height)
$graphics = [System.Drawing.Graphics]::FromImage($bitmap)
$graphics.CopyFromScreen($screen.Bounds.X, $screen.Bounds.Y, 0, 0, $screen.Bounds.Size)
$bitmap.Save("C:\Users\22798\AppData\Local\Temp\opencode\screenshot.png", [System.Drawing.Imaging.ImageFormat]::Png)
$graphics.Dispose(); $bitmap.Dispose()

# 3. 调用 image-to-markdown skill 分析截图（在对话中使用 skill 工具加载 "image-to-markdown"）

# 4. 最终构建前恢复 main.cpp（移除 window.show()）
```

**重要：** 应用启动后默认隐藏到托盘（`main.cpp` 中未调用 `window.show()`）。截图验证前必须临时添加 `window.show()`，验证后恢复。

## 崩溃调试

Windows 事件日志是主要的调试工具：

```powershell
# 查看最近的应用崩溃（最近 2 小时）
Get-WinEvent -FilterHashtable @{LogName='Application'; Level=2; StartTime=(Get-Date).AddHours(-2)} -MaxEvents 20 -ErrorAction SilentlyContinue | Where-Object { $_.Id -eq 1000 } | Format-Table TimeCreated, @{N='Exe';E={(([xml]$_.ToXml()).Event.EventData.Data)[0].'#text'}}, @{N='Module';E={(([xml]$_.ToXml()).Event.EventData.Data)[3].'#text'}} -AutoSize
```

关键崩溃码：`0xc0000005` = 访问冲突（通常是线程问题或空指针）。

## 已知陷阱

| 陷阱 | 症状 | 修复方法 |
|------|------|----------|
| DB 读取缺少 `QMutexLocker` | qsqlite.dll 中 0xc0000005 | 所有方法加锁 |
| 测试 `main()` 缺少 `QCoreApplication` | Qt6Sql.dll 启动时 0xc0000005 | 添加 `QCoreApplication app(argc, argv)` |
| WeeklyBar 标签中文本过宽 | 显示乱码如 `h.46r` | 紧凑格式（`1h46`）、更小字体（8pt）、更宽文本区域 |
| Windows 上使用 `PingFang SC` 字体 | 布局偏移、文本重叠 | 使用 `Microsoft YaHei` |
| 使用 `WA_TranslucentBackground` 但无 MICA | 客户区全黑 | 永远不要使用透明背景 |
| CircularProgress 最大值过大（24h） | 环形图看起来总是空的 | 使用 12h（43200 秒）最大值 |
| 构建时 linker permission denied | 无法打开输出文件 | 构建前 `taskkill /f /im TimeMaster.exe` |
| 运行测试缺少 MinGW/Qt PATH | `0xc0000135`（找不到 DLL） | 测试命令前设置完整 PATH + `QT_PLUGIN_PATH` |
| `WindowTracker::run()` 使用硬编码轮询间隔 | 用户配置的轮询间隔无效 | 循环中必须使用 `m_pollInterval`，非 `POLL_INTERVAL` |
| `DatabaseManager::close()` 被多次调用 | 第二次调用时 `m_db` 已无效 | 添加 `m_closed` 标志防重入 |
| `addIgnoredApp` 用 `INSERT OR IGNORE` | 重复插入返回 0（被误认为有效 ID） | 先 SELECT 查询是否存在，存在则返回已有 ID |

## SDD 流程

`.superpowers/sdd/` 包含任务简报和带审查差异的报告。实现 SDD 任务时，编写代码前先查看此目录中的规格说明。

## 语言

所有助手的回复、思考/推理过程、解释和代码注释必须使用简体中文。代码标识符（变量名、函数名等）保持英文。
