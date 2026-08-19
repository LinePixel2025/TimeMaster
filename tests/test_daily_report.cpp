#include <cassert>
#include <iostream>
#include <QCoreApplication>
#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTemporaryFile>

#include "ai/ai_client.h"
#include "database/database_manager.h"
#include "report/daily_report_manager.h"
#include "report/session_hours.h"

// 插入一条今日的会话，使今日数据非空。seconds 默认 3600（1 小时）。
static void seedTodaySession(DatabaseManager &db, int seconds = 3600)
{
    const QDateTime start(QDate::currentDate(), QTime(10, 0));
    db.insertSession(QStringLiteral("notepad.exe"), QStringLiteral("test"),
                     QStringLiteral("记事本"),
                     start, start.addSecs(seconds), seconds);
}

// 生成日报并读取全部内容，失败时断言中止。
static QByteArray generateAndRead(DailyReportManager &daily, const QString &outDir)
{
    const QString path = daily.refreshToday();
    assert(!path.isEmpty());
    assert(path.startsWith(outDir));
    QFile file(path);
    assert(file.open(QIODevice::ReadOnly));
    const QByteArray content = file.readAll();
    file.close();
    return content;
}

// 基础生成：种子今日数据后应产出含统计板块与图表的完整页面。
void test_generates_html()
{
    QTemporaryFile tmpFile;
    assert(tmpFile.open());
    const QString path = tmpFile.fileName();
    tmpFile.close();

    DatabaseManager db(path);
    seedTodaySession(db, 2 * 3600 + 1800);

    QTemporaryDir outDir;
    assert(outDir.isValid());

    AiClient ai(&db); // 未配置 AI → 统计板块仍完整，AI 区为引导空态
    DailyReportManager daily(&db, &ai);
    daily.setOutputDir(outDir.path());
    QSignalSpy readySpy(&daily, &DailyReportManager::dailyReportReady);

    const QByteArray content = generateAndRead(daily, outDir.path());
    assert(readySpy.count() == 1);
    assert(content.contains("Time Master"));
    assert(content.contains("今日使用报告"));
    assert(content.contains("今日总时长"));
    assert(content.contains("AI")); // 「AI 智能分析」区块
    assert(content.contains("<svg")); // 折线图
    assert(content.contains("记事本")); // 应用排行
    assert(content.contains("2小时30分")); // 今日总时长数值
    std::cout << "test_generates_html PASS" << std::endl;
}

// 空数据日也应生成空态页：查看操作不应失败。
void test_empty_day_still_generates()
{
    QTemporaryFile tmpFile;
    assert(tmpFile.open());
    const QString path = tmpFile.fileName();
    tmpFile.close();

    DatabaseManager db(path); // 空数据库

    QTemporaryDir outDir;
    assert(outDir.isValid());

    AiClient ai(&db);
    DailyReportManager daily(&db, &ai);
    daily.setOutputDir(outDir.path());

    const QByteArray content = generateAndRead(daily, outDir.path());
    assert(content.contains("Time Master"));
    assert(content.contains("今日总时长"));
    assert(content.contains("尚未生成")); // AI 空态引导
    std::cout << "test_empty_day_still_generates PASS" << std::endl;
}

// 同日覆盖：再次生成应覆盖为最新统计。
void test_same_day_overwrites()
{
    QTemporaryFile tmpFile;
    assert(tmpFile.open());
    const QString path = tmpFile.fileName();
    tmpFile.close();

    DatabaseManager db(path);
    seedTodaySession(db, 3600);

    QTemporaryDir outDir;
    assert(outDir.isValid());

    AiClient ai(&db);
    DailyReportManager daily(&db, &ai);
    daily.setOutputDir(outDir.path());

    const QString first = daily.refreshToday();
    assert(!first.isEmpty());

    // 数据变化后重新生成：路径不变、数值更新。
    seedTodaySession(db, 1800);
    const QString second = daily.refreshToday();
    assert(second == first);
    QFile file(second);
    assert(file.open(QIODevice::ReadOnly));
    const QByteArray content = file.readAll();
    file.close();
    assert(content.contains("1小时30分")); // 新增会话后的总时长
    std::cout << "test_same_day_overwrites PASS" << std::endl;
}

// 插入一条昨天的会话。seconds 默认 3600（1 小时）。
static void seedYesterdaySession(DatabaseManager &db, int seconds = 3600)
{
    const QDate yesterday = QDate::currentDate().addDays(-1);
    const QDateTime start(yesterday, QTime(14, 0));
    db.insertSession(QStringLiteral("notepad.exe"), QStringLiteral("test"),
                     QStringLiteral("记事本"),
                     start, start.addSecs(seconds), seconds);
}

// 昨日报告：标题、总时长与文件命名均按报告日计算，AI 区为历史日空态。
void test_yesterday_report()
{
    QTemporaryFile tmpFile;
    assert(tmpFile.open());
    const QString path = tmpFile.fileName();
    tmpFile.close();

    DatabaseManager db(path);
    seedTodaySession(db, 3600);
    seedYesterdaySession(db, 2 * 3600);

    QTemporaryDir outDir;
    assert(outDir.isValid());

    AiClient ai(&db);
    DailyReportManager daily(&db, &ai);
    daily.setOutputDir(outDir.path());

    const QDate yesterday = QDate::currentDate().addDays(-1);
    const QString reportPath = daily.refreshDay(yesterday);
    assert(!reportPath.isEmpty());
    assert(reportPath.endsWith(QStringLiteral("日报-")
        + yesterday.toString(QStringLiteral("yyyy-MM-dd"))
        + QStringLiteral(".html")));

    QFile file(reportPath);
    assert(file.open(QIODevice::ReadOnly));
    const QByteArray content = file.readAll();
    file.close();
    assert(content.contains("昨日使用报告"));
    assert(content.contains("昨日总时长"));
    assert(content.contains("2小时")); // 昨日总时长数值
    assert(!content.contains("今日使用报告"));
    // 无昨日锚点的 AI 缓存 → 历史日空态说明。
    assert(content.contains("历史日报告以统计板块为准"));

    // 今日报告不受影响。
    const QByteArray todayContent = generateAndRead(daily, outDir.path());
    assert(todayContent.contains("今日使用报告"));
    std::cout << "test_yesterday_report PASS" << std::endl;
}

// 跨小时会话必须按占用分摊，不能整段落在起始小时。
void test_session_hours_split_across_hour()
{
    int hours[24] = {};
    int periods[4] = {};
    const QDate today = QDate::currentDate();
    const QDateTime start(today, QTime(10, 30));
    SessionHours::addToDayHours(start, 3600, today, hours, periods);
    assert(hours[10] == 1800);
    assert(hours[11] == 1800);
    assert(periods[1] == 3600);
    std::cout << "test_session_hours_split_across_hour PASS" << std::endl;
}

// AI 分析回填：applyReportText 后页面应包含注入的文本（Markdown 转换后）。
void test_apply_report_text()
{
    QTemporaryFile tmpFile;
    assert(tmpFile.open());
    const QString path = tmpFile.fileName();
    tmpFile.close();

    DatabaseManager db(path);
    seedTodaySession(db);

    QTemporaryDir outDir;
    assert(outDir.isValid());

    AiClient ai(&db);
    DailyReportManager daily(&db, &ai);
    daily.setOutputDir(outDir.path());

    daily.refreshToday();
    daily.applyReportText(QStringLiteral("## 概览\n今日状态平稳，注意休息。"));

    QFile file(QDir(outDir.path()).filePath(
        QStringLiteral("日报-") + QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd"))
            + QStringLiteral(".html")));
    assert(file.open(QIODevice::ReadOnly));
    const QByteArray content = file.readAll();
    file.close();
    assert(content.contains("<h3>概览</h3>"));
    assert(content.contains("今日状态平稳"));
    assert(content.contains("AI 分析生成于")); // 锚点日期标注
    std::cout << "test_apply_report_text PASS" << std::endl;
}

// 每次生成重读缓存：数据库里的 AI 缓存被外部更新后，刷新页面应立即采用最新文本。
void test_refresh_reads_latest_ai_cache()
{
    QTemporaryFile tmpFile;
    assert(tmpFile.open());
    const QString path = tmpFile.fileName();
    tmpFile.close();

    DatabaseManager db(path);
    seedTodaySession(db);

    QTemporaryDir outDir;
    assert(outDir.isValid());

    AiClient ai(&db);
    DailyReportManager daily(&db, &ai);
    daily.setOutputDir(outDir.path());

    daily.refreshToday();

    // 模拟 AI 在别处完成并写库（saveCache 的写入内容）。
    db.setSetting(QStringLiteral("ai_report_daily_text"),
                  QStringLiteral("## 概览\n最新的外部分析。"));
    db.setSetting(QStringLiteral("ai_report_daily_date"),
                  QDate::currentDate().toString(Qt::ISODate));

    const QString reportPath = daily.refreshToday();
    QFile file(reportPath);
    assert(file.open(QIODevice::ReadOnly));
    const QByteArray content = file.readAll();
    file.close();
    assert(content.contains("最新的外部分析"));
    assert(content.contains("AI 分析生成于"));
    std::cout << "test_refresh_reads_latest_ai_cache PASS" << std::endl;
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    test_generates_html();
    test_empty_day_still_generates();
    test_same_day_overwrites();
    test_yesterday_report();
    test_session_hours_split_across_hour();
    test_apply_report_text();
    test_refresh_reads_latest_ai_cache();
    std::cout << "All daily report tests passed!" << std::endl;
    return 0;
}
