# Time Master C++ Rewrite — Design Spec

## Overview

Rewrite the existing Python/PySide6 "Time Master" desktop application in C++ with Qt6, preserving all features while improving performance and reducing runtime dependencies. The rewrite targets Windows only (same as original).

## Tech Stack

| Component | Choice | Rationale |
|---|---|---|
| Language | C++17 | Modern, widely supported, matches Qt6 requirements |
| GUI | Qt6 (Widgets) | Direct translation from PySide6, same visual fidelity |
| Build | CMake 3.22+ | Standard C++ build system |
| Package mgr | vcpkg | USTC mirror for Chinese users |
| Database | Qt6 Sql (SQLite driver) | Same SQLite, thread-safe with QMutex |
| Window tracking | Win32 API | Replace pywin32 + psutil entirely |
| Export CSV | C++ fstream | Standard library |
| Export XLSX | Hand-written XlsxWriter (miniz) | Avoid external library dependency |

## Project Structure

```
TimeMaster/
├── CMakeLists.txt                     # Root CMake configuration
├── vcpkg.json                         # vcpkg manifest
├── CMakePresets.json                  # USTC mirror preset
├── src/
│   ├── main.cpp                       # QApplication + entry point
│   ├── database/
│   │   ├── database_manager.h
│   │   └── database_manager.cpp       # QSqlDatabase wrapper
│   ├── tracker/
│   │   ├── window_tracker.h
│   │   └── window_tracker.cpp         # QThread subclass, polls foreground window
│   ├── ui/
│   │   ├── main_window.h/.cpp         # Main window with Mica/acrylic backdrop
│   │   ├── stats_widget.h/.cpp        # CircularProgress + WeeklyBar + GlassCard
│   │   ├── app_rank_widget.h/.cpp     # App ranking list with progress bars
│   │   └── tray_manager.h/.cpp        # QSystemTrayIcon manager
│   └── export/
│       ├── exporter.h/.cpp            # CSV + XLSX export
│       └── xlsx_writer.h/.cpp         # Minimal XLSX (ZIP+XML, uses miniz)
├── resources/
│   ├── icon.ico
│   ├── icon.png
│   └── resources.qrc
├── tests/
│   ├── CMakeLists.txt
│   ├── test_database.cpp
│   ├── test_window_tracker.cpp
│   └── test_exporter.cpp
└── third_party/
    └── miniz/                         # Single-file ZIP library (bundled)
```

## Build & Mirror Configuration

### vcpkg.json
```json
{
  "name": "timemaster",
  "version": "1.0.0",
  "dependencies": [
    "qt6-base",
    "qt6-tools"
  ]
}
```

### CMakePresets.json (USTC mirror)
```json
{
  "version": 6,
  "configurePresets": [
    {
      "name": "default",
      "displayName": "Default (USTC mirror)",
      "generator": "Visual Studio 17 2022",
      "binaryDir": "${sourceDir}/build",
      "cacheVariables": {
        "CMAKE_TOOLCHAIN_FILE": "$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake",
        "X_VCPKG_ASSET_SOURCES": "x-azurl,https://mirrors.ustc.edu.cn/vcpkg/"
      }
    }
  ]
}
```

## Component Details

### DatabaseManager (`src/database/`)
- Wraps `QSqlDatabase` with SQLite driver
- Thread-safe: `QMutex` on write operations, unlocked reads
- Schema: identical `sessions` table (id, process_name, window_title, app_name, start_time, end_time, duration_seconds)
- API: `insertSession()`, `updateSessionEnd()`, `getTodaySummary()`, `getTodayTotal()`, `getWeekSummary()`, `getAppRank()`, `getAllSessions()`, `getDailySummaries()`
- Database path: `%APPDATA%/Time Master/data.db`

### WindowTracker (`src/tracker/`)
- `QThread` subclass polling every 1 second
- Uses Win32 API: `GetForegroundWindow()` → `GetWindowThreadProcessId()` → `OpenProcess()` → `QueryFullProcessImageNameW()` → `GetFilePartW()`
- `APP_NAME_MAP` as `std::unordered_map<std::wstring, std::wstring>` (same 40+ app mappings)
- `classifyApp()` function to map process names to display names
- Idle detection: 60s threshold, emits `idleChanged` signal
- Session tracking: tracks pid + window title changes, writes session start to DB, updates end on switch

### MainWindow (`src/ui/`)
- `QMainWindow` subclass
- Applies Mica (Win11) or Acrylic (Win10) backdrop via `DwmSetWindowAttribute`
- Dark mode via `DwmSetWindowAttribute` with `DWMWA_USE_IMMERSIVE_DARK_MODE`
- Contains `StatsWidget`, `AppRankWidget`, export/refresh buttons
- Auto-refresh `QTimer` every 10s
- Close hides window instead of quitting (system tray behavior)

### StatsWidget (`src/ui/`)
- Contains `CircularProgress` (custom QPainter widget, 140x140, gradient arc)
- Contains `WeeklyBar` (custom QPainter widget, 7-day bar chart)
- `GlassCard` QFrame subclass with translucent gradient background

### AppRankWidget (`src/ui/`)
- `AppRankItem` custom QPainter widget for each rank row
- Displays rank number, app name, gradient progress bar, time string

### TrayManager (`src/ui/`)
- `QSystemTrayIcon` with context menu (Show/Hide/Quit)
- Double-click to show window
- Tooltip updated every 10s with today's total time
- Signals: `showMainWindow`, `quitApp`

### Exporter (`src/export/`)
- `Exporter` class wrapping `DatabaseManager` methods
- `exportCsv()`: writes UTF-8 BOM CSV via `std::ofstream`
- `exportExcel()`: uses `XlsxWriter` to create XLSX (ZIP containing XML)
- Three sheets: Usage Records, Daily Summary, App Ranking

### XlsxWriter (`src/export/`)
- Hand-written XLSX writer (~200 lines)
- Uses `miniz` (single-file `miniz.c`/`miniz.h` bundled in `third_party/`) for ZIP creation
- Generates `[Content_Types].xml`, `_rels/.rels`, `xl/workbook.xml`, `xl/_rels/workbook.xml.rels`, `xl/worksheets/sheet*.xml`, `xl/sharedStrings.xml`
- Supports: bold headers, multiple sheets, auto-width columns, string/number cells

## Data Flow

1. **WindowTracker thread**: every 1s polls foreground window → classifies app → writes/updates session in DB
2. **GUI thread timers**: every 10s reads today total + week summary + app rank → updates widgets
3. **Export**: on-demand read of all sessions → write CSV or XLSX file

All DB writes happen in the tracker thread; all reads happen in the GUI thread. `QMutex` protects write paths.

## Thread Safety

- DatabaseManager: `QMutex` on `insertSession()`, `updateSessionEnd()`; reads are lock-free
- WindowTracker signals (`activeWindowChanged`, `idleChanged`) are auto-queued to GUI thread via Qt's signal-slot mechanism
- No shared mutable state between threads beyond the database

## Testing Strategy

- `test_database.cpp`: test SQLite operations with temporary DB files (same as Python tests)
- `test_window_tracker.cpp`: test `classifyApp()` function; window tracking requires actual Windows environment so it's integration-test only
- `test_exporter.cpp`: test CSV and XLSX export to temp files, verify output
- Tests use Qt Test framework (`QTest`) for GUI tests where applicable
- Test build: separate `tests/CMakeLists.txt` with `BUILD_TESTING` option

## What We Keep From Python

- Same visual design (gradient colors, PingFang SC font, glassmorphism cards, dark theme)
- Same APP_NAME_MAP (40+ app mappings)
- Same SQLite schema
- Same system tray behavior
- Same export format (CSV + XLSX with Chinese headers)

## What Changes

- Python runtime → compiled C++ binary
- PySide6 → Qt6 C++ API (very similar, almost line-by-line mapping)
- psutil → Win32 `CreateToolhelp32Snapshot` + `QueryFullProcessImageNameW`
- openpyxl → hand-written XlsxWriter
- `threading.Lock` → `QMutex`

## Non-Goals

- Cross-platform support (Windows only, same as original)
- Performance optimization beyond what the port naturally provides
- New features not present in the Python version
- Icon generation (keep existing `resources/icon.ico` and `resources/icon.png`)
