#include <QApplication>
#include <QFont>
#include <QTimer>
#include "database/database_manager.h"
#include "tracker/window_tracker.h"
#include "ui/main_window.h"
#include "ui/tray_manager.h"
#include "utility/autostart_helper.h"
#include "push/lineweb_pusher.h"
#include "ui/theme_manager.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("Time Master");
    app.setOrganizationName("TimeMaster");
    app.setApplicationVersion(APP_VERSION);
    app.setWindowIcon(QIcon(":/icon.svg"));
    app.setAttribute(Qt::AA_DontCreateNativeWidgetSiblings);

    // Set global font
    QFont font("Microsoft YaHei", 10);
    font.setStyleStrategy(QFont::PreferAntialias);
    app.setFont(font);

    DatabaseManager db;

    ThemeManager::instance()->loadFromDb(&db);

    bool autoStartSetting = (db.getSetting("auto_start", "false") == "true");
    if (autoStartSetting != AutoStartHelper::isAutoStartEnabled())
        AutoStartHelper::setAutoStart(autoStartSetting);

    LineWebPusher pusher(&db);

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
        pusher.stop();
        pusher.pushNow();
        tracker.stop();
        tracker.wait(10000);
        db.close();
        app.quit();
    });

    // 云端目标写回后刷新仪表盘（HeroCard 环与文案随目标更新）。
    QObject::connect(&pusher, &LineWebPusher::goalUpdated,
                     &window, &MainWindow::refreshData);

    // 当日首次超目标弹一次托盘提醒。
    QObject::connect(&pusher, &LineWebPusher::goalExceeded, [&tray](int overMinutes) {
        tray.showNotification(
            QString::fromUtf8("Time Master \xe6\x97\xb6\xe9\x97\xb4\xe6\x8f\x90\xe9\x86\x92"),
            QString::fromUtf8("\xe4\xbb\x8a\xe6\x97\xa5\xe5\xb7\xb2\xe8\xb6\x85\xe7\x9b\xae\xe6\xa0\x87 %1 \xe5\x88\x86\xe9\x92\x9f")
                .arg(overMinutes));
    });

    tray.show();
    window.refreshData();

    QObject::connect(&window, &MainWindow::settingsChanged, &pusher, &LineWebPusher::reloadSettings);

    QObject::connect(&window, &MainWindow::settingsChanged, [&]() {
        tracker.reloadSettings();
    });

    pusher.start();

    return app.exec();
}
