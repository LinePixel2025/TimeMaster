### Task 10: main.cpp Entry Point

**Files:**
- Modify: `src/main.cpp`

**Interfaces:**
- Consumes: All components
- Produces: Complete application

- [ ] **Step 1: Rewrite main.cpp**

```cpp
#include <QApplication>
#include <QFont>
#include <QTimer>
#include "database/database_manager.h"
#include "tracker/window_tracker.h"
#include "ui/main_window.h"
#include "ui/tray_manager.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("Time Master");
    app.setOrganizationName("TimeMaster");
    app.setAttribute(Qt::AA_DontCreateNativeWidgetSiblings);

    // Set global font
    QFont font("PingFang SC", 10);
    font.setStyleStrategy(QFont::PreferAntialias);
    app.setFont(font);

    DatabaseManager db;

    MainWindow window(&db);

    WindowTracker tracker(&db);
    tracker.start();

    TrayManager tray("Time Master");
    QObject::connect(&tray, &TrayManager::showMainWindow, [&]() {
        window.show();
        window.refreshData();
    });

    // Update tray tooltip every 10 seconds
    QTimer tooltipTimer;
    auto updateTooltip = [&]() {
        int total = db.getTodayTotal();
        int hours = total / 3600;
        int minutes = (total % 3600) / 60;
        tray.setTooltip(QString("Time Master - %1 %2h %3m")
            .arg(QString::fromUtf8("\xe4\xbb\x8a\xe6\x97\xa5\xe5\xb7\xb2\xe7\x94\xa8"))
            .arg(hours).arg(minutes));
    };
    QObject::connect(&tooltipTimer, &QTimer::timeout, updateTooltip);
    tooltipTimer.setInterval(10000);
    tooltipTimer.start();
    updateTooltip();

    QObject::connect(&tray, &TrayManager::quitApp, [&]() {
        tracker.stop();
        tracker.wait(2000);
        db.close();
        app.quit();
    });

    tray.show();
    window.refreshData();

    return app.exec();
}
```

- [ ] **Step 2: Build the full project**

```bash
cmake --build build
```

Expected: Clean build, no errors, `TimeMaster.exe` is produced.

- [ ] **Step 3: Commit**

```bash
git add -A
git commit -m "feat: complete main.cpp wiring all components together"
```

---

