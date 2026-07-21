# Task 7 Report: Wire settingsChanged signal in main.cpp

**Status:** Complete
**Base:** 10eb6b6

## Changes

**File modified:** `src/main.cpp`

1. Added `tracker.reloadSettings()` call after `tracker.start()` (line 27) to load initial settings (ignored apps, aliases) when the tracker starts.

2. Connected `MainWindow::settingsChanged` signal to a lambda that calls `tracker.reloadSettings()` (lines 60-62), so that when the user saves changes in SettingsDialog, the tracker immediately picks up the new configuration.

## Build

```
cmake --build build → SUCCESS
```

No warnings or errors.
