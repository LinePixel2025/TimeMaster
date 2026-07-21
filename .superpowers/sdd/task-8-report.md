# Task 8: AppRankWidget — Report

**Status:** ✓ Complete

## Files Changed
- `src/ui/app_rank_widget.h` — Added `AppRankItem` and `AppRankWidget` class declarations with `DatabaseManager` dependency
- `src/ui/app_rank_widget.cpp` — Full implementation with custom-painted rank items (gradient bar, rank number, app name, time string) and `refresh()` logic

## Implementation Notes
- `AppRankItem`: Custom-painted `QWidget` with rank number, app name, proportional gradient bar (`#818CF8` → `#6366F1`), and formatted time string (`Xh Ym` / `Xm`).
- `AppRankWidget`: `QFrame` with title label ("应用使用排行 (今日)") and scrollable list. `refresh()` clears existing items and rebuilds from `DatabaseManager::getAppRank()`.

## Commit
`b6f61c5` — `feat: AppRankWidget with per-app time ranking list`
