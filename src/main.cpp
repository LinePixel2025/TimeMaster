#include <QApplication>
#include <QFont>
#include <QTimer>
#include <QMessageBox>
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

    // 主界面「云端同步」按钮：立即推送补推/今日并拉取云端状态。
    QObject::connect(&window, &MainWindow::cloudSyncRequested, [&]() {
        if (!pusher.syncNow()) {
            QMessageBox::warning(&window,
                QString::fromUtf8("\xe4\xba\x91\xe7\xab\xaf\xe5\x90\x8c\xe6\xad\xa5\xe6\x9c\xaa\xe9\x85\x8d\xe7\xbd\xae"),
                QString::fromUtf8("\xe8\xaf\xb7\xe5\x85\x88\xe5\x9c\xa8\xe3\x80\x8c\xe8\xae\xbe\xe7\xbd\xae \xe2\x86\x92 \xe4\xba\x91\xe7\xab\xaf\xe5\x90\x8c\xe6\xad\xa5\xe3\x80\x8d\xe4\xb8\xad\xe5\x90\xaf\xe7\x94\xa8\xe6\x8e\xa8\xe9\x80\x81\xe5\xb9\xb6\xe5\xa1\xab\xe5\x86\x99 API \xe5\x9c\xb0\xe5\x9d\x80\xe4\xb8\x8e Token\xe3\x80\x82"));
        }
    });
    // 同步/推送成功后刷新仪表盘（拉取到的云端目标/时长立即生效）。
    QObject::connect(&pusher, &LineWebPusher::pushSucceeded, &window, &MainWindow::refreshData);
    // 推送失败（含后台周期推送）通过托盘气泡提示，不打断操作。
    QObject::connect(&pusher, &LineWebPusher::pushFailed, [&tray](const QString &err) {
        tray.showNotification(
            QString::fromUtf8("\xe4\xba\x91\xe7\xab\xaf\xe5\x90\x8c\xe6\xad\xa5\xe5\xa4\xb1\xe8\xb4\xa5"),
            QString::fromUtf8("\xe6\x8e\xa8\xe9\x80\x81\xe5\xa4\xb1\xe8\xb4\xa5\xef\xbc\x9a%1").arg(err));
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
