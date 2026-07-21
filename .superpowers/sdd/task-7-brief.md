# Task 7: Wire settingsChanged signal in main.cpp

**Files:**
- Modify: `src/main.cpp`

## Requirement

In `src/main.cpp`, after the `quitApp` connection block (where `tracker.stop()` is called), and after `tracker.start()`, add a call to load initial settings and a connection between MainWindow's settingsChanged signal and tracker's reloadSettings().

### Code to add

After `tracker.start();` (line ~26), add:

```cpp
    tracker.reloadSettings();
```

After the `quitApp` connection block (the `window.refreshData()` line is already there), add:

```cpp
    QObject::connect(&window, &MainWindow::settingsChanged, [&]() {
        tracker.reloadSettings();
    });
```

The result around lines 25-60 should look like (showing relevant context):

```cpp
    WindowTracker tracker(&db);
    tracker.start();
    tracker.reloadSettings();

    TrayManager tray("Time Master");
    // ... existing tray connections ...

    tray.show();
    window.refreshData();

    QObject::connect(&window, &MainWindow::settingsChanged, [&]() {
        tracker.reloadSettings();
    });

    return app.exec();
```

## Verification

```powershell
$env:PATH = "D:\AICOP\requirements\QT6\Tools\mingw1310_64\bin;D:\AICOP\requirements\QT6\6.11.1\mingw_64\bin;D:\AICOP\requirements\QT6\Tools\Ninja;D:\AICOP\requirements\QT6\Tools\CMake_64\bin;$env:PATH"
cmake --build build
```

Expected: compiles and links.

## Global Constraints

- All `DatabaseManager` methods must acquire `QMutexLocker lock(&m_mutex)`
- Font: `Microsoft YaHei`, background: `#F0F2F5`
- No MICA/transparent backgrounds
