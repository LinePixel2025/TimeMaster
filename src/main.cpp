#include <QApplication>
#include <QFont>
#include <QTimer>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>
#include "database/database_manager.h"
#include "tracker/window_tracker.h"
#include "ui/main_window.h"
#include "ui/tray_manager.h"
#include "utility/autostart_helper.h"
#include "push/lineweb_pusher.h"
#include "ai/ai_client.h"
#include "reminder/reminder_scheduler.h"
#include "reminder/weekly_report_manager.h"
#include "report/daily_report_manager.h"
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

    AiClient ai(&db);
    ai.reloadSettings();

    ReminderScheduler scheduler(&db, &ai);

    WeeklyReportManager weekly(&db, &ai);

    DailyReportManager daily(&db, &ai);

    MainWindow window(&db, &ai);

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
        scheduler.stop();
        weekly.stop();
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

    // 主页 AI 报告卡片：点击生成 → 调用 AI；结果回填卡片与日报网页。
    // generateReport 返回 false 时（如该周期数据清零）立即回填失败，避免卡片
    // 停留在生成中状态。dailyManualPending 标记手动请求，AI 完成后自动打开日报。
    bool dailyManualPending = false;
    QString dailyAiUsageText; // 本次手动生成的 token 消耗，随完成气泡展示后失效
    QObject::connect(&window, &MainWindow::aiReportRequested, &window,
                     [&](const QString &period) {
        dailyManualPending = true;
        dailyAiUsageText.clear();
        if (!ai.generateReport(period)) {
            dailyManualPending = false;
            window.onAiReportFailed(
                period,
                QString::fromUtf8("\xe8\xaf\xa5\xe5\x91\xa8\xe6\x9c\x9f\xe5\xb0\x9a\xe6\x97\xa0\xe4\xbd\xbf\xe7\x94\xa8\xe6\x95\xb0\xe6\x8d\xae"));
        }
    });
    QObject::connect(&ai, &AiClient::reportReady,
                     &window, &MainWindow::onAiReportReady);
    QObject::connect(&ai, &AiClient::reportFailed,
                     &window, &MainWindow::onAiReportFailed);
    // AI 分析回填日报：手动请求（⟳）完成后网页自动更新并在浏览器打开。
    QObject::connect(&ai, &AiClient::reportReady,
                     [&](const QString &period, const QString &text,
                         int promptTokens, int completionTokens,
                         int totalTokens) {
        if (period == AiPeriod::daily()) {
            daily.applyReportText(text);
            dailyAiUsageText = AiClient::formatUsageText(
                promptTokens, completionTokens, totalTokens);
        }
    });
    QObject::connect(&ai, &AiClient::reportFailed,
                     [&](const QString &period, const QString &) {
        if (period == AiPeriod::daily()) {
            dailyManualPending = false; // 失败不自动打开，卡片显示错误
            dailyAiUsageText.clear();
        }
    });
    QObject::connect(&daily, &DailyReportManager::dailyReportReady,
                     [&tray, &dailyManualPending, &dailyAiUsageText](
                         const QString &path) {
        if (dailyManualPending) {
            dailyManualPending = false;
            QDesktopServices::openUrl(QUrl::fromLocalFile(path));
            // 接口未返回 usage 时 formatUsageText 为空串，气泡保持原文案。
            QString msg = QString::fromUtf8(
                "\xe5\xb7\xb2\xe5\x9c\xa8\xe6\xb5\x8f\xe8\xa7\x88\xe5\x99\xa8"
                "\xe4\xb8\xad\xe6\x89\x93\xe5\xbc\x80\xe3\x80\x82");
            msg += dailyAiUsageText;
            dailyAiUsageText.clear();
            tray.showNotification(
                QString::fromUtf8("\xe4\xbb\x8a\xe6\x97\xa5\xe6\x8a\xa5\xe5\x91\x8a\xe5\xb7\xb2\xe7\x94\x9f\xe6\x88\x90"),
                msg);
        }
    });
    // 主页「↗」：现场生成最新统计的今日报告网页并在浏览器打开
    //（AI 未配置/无缓存时统计板块仍然完整）。
    QObject::connect(&window, &MainWindow::dailyReportOpenRequested, [&]() {
        const QString path = daily.refreshToday();
        if (!path.isEmpty())
            QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    });

    // 定时提醒：到点经托盘气泡弹出（内容由调度器本地模板或 AI 回退生成）。
    QObject::connect(&scheduler, &ReminderScheduler::reminderDue,
                     [&tray](const QString &title, const QString &message) {
        tray.showNotification(title, message);
    });

    // 每周周报：生成成功后回填主页按钮；手动触发的生成自动在浏览器打开，
    // 自动定时生成仅托盘通知（避免无预期地弹出浏览器）。
    bool manualWeeklyPending = false;
    QObject::connect(&weekly, &WeeklyReportManager::weeklyReportReady,
                     [&tray, &window, &manualWeeklyPending](
                         const QString &path, const QString &aiUsage) {
        window.onWeeklyReportReady(path);
        // aiUsage 为本次 AI 分析的 token 消耗（本地小结回退时为空）。
        QString msg;
        if (manualWeeklyPending) {
            manualWeeklyPending = false;
            QDesktopServices::openUrl(QUrl::fromLocalFile(path));
            msg = QString::fromUtf8("\xe5\xb7\xb2\xe5\x9c\xa8\xe6\xb5\x8f\xe8\xa7\x88\xe5\x99\xa8\xe4\xb8\xad\xe6\x89\x93\xe5\xbc\x80\xe3\x80\x82");
        } else {
            msg = QString::fromUtf8("\xe5\xb7\xb2\xe4\xbf\x9d\xe5\xad\x98\xef\xbc\x9a%1").arg(path);
        }
        tray.showNotification(
            QString::fromUtf8("\xe4\xb8\x8a\xe5\x91\xa8\xe5\x91\xa8\xe6\x8a\xa5\xe5\xb7\xb2\xe7\x94\x9f\xe6\x88\x90"),
            msg + aiUsage);
    });
    // 主页「上周周报」按钮：在系统浏览器打开 HTML。
    QObject::connect(&window, &MainWindow::weeklyReportOpenRequested,
                     [](const QString &path) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    });
    // 启动时回填已存在的周报路径（历史生成过的周报在弹窗内可打开）。
    window.onWeeklyReportReady(
        db.getSetting("weekly_report_path", ""));
    // 主页「立即生成上周周报」按钮：手动触发生成（同周已生成过则打开已有文件）；
    // 无数据时提示，不影响其他功能。generateNow 对「已有文件」场景会同步 emit
    // ready，因此 flag 需在调用前置位。
    QObject::connect(&window, &MainWindow::weeklyReportGenerateRequested, [&]() {
        manualWeeklyPending = true;
        if (!weekly.generateNow()) {
            manualWeeklyPending = false;
            QMessageBox::warning(&window,
                QString::fromUtf8("\xe4\xb8\x8a\xe5\x91\xa8\xe5\x91\xa8\xe6\x8a\xa5\xe6\x9c\xaa\xe7\x94\x9f\xe6\x88\x90"),
                QString::fromUtf8("\xe4\xb8\x8a\xe5\x91\xa8\xe6\x97\xa0\xe4\xbd\xbf\xe7\x94\xa8\xe6\x95\xb0\xe6\x8d\xae\xef\xbc\x8c\xe6\x97\xa0\xe6\xb3\x95\xe7\x94\x9f\xe6\x88\x90\xe5\x91\xa8\xe6\x8a\xa5\xe3\x80\x82"));
            return;
        }
        // AI 已配置且请求在途：分析最多需要约 90 秒，先给出反馈避免误以为无响应。
        if (weekly.isAiPending()) {
            tray.showNotification(
                QString::fromUtf8("\xe5\x91\xa8\xe6\x8a\xa5\xe7\x94\x9f\xe6\x88\x90\xe4\xb8\xad"),
                QString::fromUtf8("AI \xe5\x88\x86\xe6\x9e\x90\xe8\xbf\x9b\xe8\xa1\x8c\xe4\xb8\xad\xef\xbc\x8c\xe5\xae\x8c\xe6\x88\x90\xe5\x90\x8e\xe5\xb0\x86\xe8\x87\xaa\xe5\x8a\xa8\xe6\x89\x93\xe5\xbc\x80\xe6\x8a\xa5\xe5\x91\x8a\xe3\x80\x82"));
        }
    });
    // 主页「重新生成」按钮：清除上周去重键，强制重新统计并覆盖已有周报。
    QObject::connect(&window, &MainWindow::weeklyReportRegenerateRequested, [&]() {
        manualWeeklyPending = true;
        if (!weekly.regenerateNow()) {
            manualWeeklyPending = false;
            QMessageBox::warning(&window,
                QString::fromUtf8("\xe5\x91\xa8\xe6\x8a\xa5\xe6\x9c\xaa\xe7\x94\x9f\xe6\x88\x90"),
                QString::fromUtf8("\xe4\xb8\x8a\xe5\x91\xa8\xe6\x97\xa0\xe4\xbd\xbf\xe7\x94\xa8\xe6\x95\xb0\xe6\x8d\xae\xef\xbc\x8c\xe6\x97\xa0\xe6\xb3\x95\xe7\x94\x9f\xe6\x88\x90\xe5\x91\xa8\xe6\x8a\xa5\xe3\x80\x82"));
            return;
        }
        if (weekly.isAiPending()) {
            tray.showNotification(
                QString::fromUtf8("\xe5\x91\xa8\xe6\x8a\xa5\xe7\x94\x9f\xe6\x88\x90\xe4\xb8\xad"),
                QString::fromUtf8("AI \xe5\x88\x86\xe6\x9e\x90\xe8\xbf\x9b\xe8\xa1\x8c\xe4\xb8\xad\xef\xbc\x8c\xe5\xae\x8c\xe6\x88\x90\xe5\x90\x8e\xe5\xb0\x86\xe8\x87\xaa\xe5\x8a\xa8\xe6\x89\x93\xe5\xbc\x80\xe6\x8a\xa5\xe5\x91\x8a\xe3\x80\x82"));
        }
    });

    tray.show();
    window.refreshData();

    QObject::connect(&window, &MainWindow::settingsChanged, &pusher, &LineWebPusher::reloadSettings);

    QObject::connect(&window, &MainWindow::settingsChanged, [&]() {
        tracker.reloadSettings();
    });

    QObject::connect(&window, &MainWindow::settingsChanged, &ai, &AiClient::reloadSettings);

    QObject::connect(&window, &MainWindow::settingsChanged, &scheduler, &ReminderScheduler::reloadSettings);

    QObject::connect(&window, &MainWindow::settingsChanged, &weekly, &WeeklyReportManager::reloadSettings);

    pusher.start();
    scheduler.start();
    weekly.start();

    return app.exec();
}
