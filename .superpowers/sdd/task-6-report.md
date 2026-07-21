# Task 6: WindowTracker filtering + aliases + reloadSettings - Report

## Completed

### window_tracker.h
- Added `#include <QSet>` for `QSet<QString>` type
- Removed `static` from `classifyApp` (required to access member `m_aliases`)
- Added `void reloadSettings()` public method declaration
- Added member variables: `m_ignoredApps`, `m_aliases`, `m_trackingEnabled`, `m_pollInterval`, `m_idleThreshold`

### window_tracker.cpp
- **Constructor**: Calls `reloadSettings()` to initialize settings from DB
- **classifyApp**: Checks `m_aliases` map first before falling through to `APP_NAME_MAP` hardcoded lookup
- **tick()**: Early-returns if `m_trackingEnabled` is false, or if the foreground process name is in `m_ignoredApps`
- **reloadSettings()**: Loads `tracking_enabled`, `poll_interval`, `idle_threshold` from DB settings, populates `m_ignoredApps` from `getIgnoredApps()`, and `m_aliases` from `getAppAliases()`

### Build
Compiles and links successfully with no errors.

## Deviation from brief

Removed `static` from `classifyApp` — the new implementation accesses `m_aliases` (a member variable), so `static` is incompatible. No external callers exist; only `getForegroundWindowInfo()` within the same class calls it.
