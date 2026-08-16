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

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    test_generates_html();
    test_empty_day_still_generates();
    test_same_day_overwrites();
    test_apply_report_text();
    std::cout << "All daily report tests passed!" << std::endl;
    return 0;
}
