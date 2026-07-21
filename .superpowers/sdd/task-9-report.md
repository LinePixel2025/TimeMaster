# Task 9: MainWindow — Report

## Status: Complete

## Changes
- **src/ui/main_window.h** — Overwritten with full declaration: `MainWindow` inherits `QMainWindow`, holds pointers to `DatabaseManager`, `StatsWidget`, `AppRankWidget`, and a `QTimer` for auto-refresh. Exposes `refreshData()` slot, overrides `showEvent`/`closeEvent`, private slots `applyBackdrop()` and `onExport()`.
- **src/ui/main_window.cpp** — Full implementation:
  - Mica (Win 11 ≥ 22000) or Acrylic backdrop via `DwmSetWindowAttribute`
  - Dark mode forced via `DWMWA_USE_IMMERSIVE_DARK_MODE`
  - Layout: `StatsWidget` + `AppRankWidget` stacked vertically, export/refresh buttons
  - Auto-refresh every 10 seconds via `QTimer`
  - Export dialog: format selection (`CSV`/`Excel`) → `QFileDialog` → `Exporter` with success/error messages
  - `closeEvent` hides window instead of closing

## Commit
`4e82368` — `feat: MainWindow with Mica backdrop and widget integration`
