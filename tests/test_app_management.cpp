#include <cassert>
#include <iostream>
#include <QCoreApplication>
#include <QDateTime>
#include <QDate>
#include <QDir>
#include <QFile>
#include <QMap>
#include <QScopedPointer>
#include <QString>
#include <QTime>
#include <QVector>
#include <QVariantMap>
#include "database/database_manager.h"
#include "ui/group_icons.h"

namespace {

QString tempDbPath()
{
    static int counter = 0;
    return QDir::temp().filePath(
        QStringLiteral("tm_appmgmt_%1_%2.db")
            .arg(QCoreApplication::applicationPid())
            .arg(++counter));
}

/// 每个用例一份独立临时库，避免用例之间共享 sessions。
DatabaseManager *makeDb(QString *pathOut = nullptr)
{
    const QString path = tempDbPath();
    QFile::remove(path);
    if (pathOut)
        *pathOut = path;
    return new DatabaseManager(path);
}

/// 插入一条达到最短记录阈值（默认 40s）的今日会话。
void addSession(DatabaseManager *db, const QString &processPath,
                const QString &appName, int seconds)
{
    const QDateTime now = QDateTime::currentDateTime();
    const qint64 id = db->insertSession(processPath, QStringLiteral("win"),
                                        appName, now, now, seconds);
    assert(id > 0);
}

const char *kChrome = "C:\\Program Files\\Chrome\\chrome.exe";
const char *kCode = "C:\\Program Files\\Code\\Code.exe";
const char *kWechat = "C:\\Program Files\\WeChat\\WeChat.exe";
const char *kSteam = "C:\\Program Files\\Steam\\steam.exe";

} // namespace

void test_preset_groups_seeded_once()
{
    QString path;
    QScopedPointer<DatabaseManager> db(makeDb(&path));

    QVector<QVariantMap> groups = db->getGroups();
    assert(groups.size() == 6);
    assert(groups[0]["name"].toString() == QString::fromUtf8("开发效率"));
    assert(groups[0]["members"].toInt() == 0);
    assert(groups[0]["builtin"].toBool());
    // 预设组别带固定图标（emoji），非空且与预设表一致。
    assert(groups[0]["icon"].toString() == QString::fromUtf8("💻"));
    for (const QVariantMap &row : groups)
        assert(row["icon"].toString() == GroupIcons::presetIconFor(row["name"].toString())
               && !row["icon"].toString().isEmpty());

    // 删掉一个预设后重新打开数据库，不应被重新播种。
    db->removeGroup(groups[0]["id"].toInt());
    assert(db->getGroups().size() == 5);
    db->close();

    DatabaseManager reopened(path);
    QVector<QVariantMap> after = reopened.getGroups();
    assert(after.size() == 5);
    for (const QVariantMap &row : after)
        assert(row["name"].toString() != QString::fromUtf8("开发效率"));

    std::cout << "test_preset_groups_seeded_once PASS" << std::endl;
}

void test_group_crud()
{
    QScopedPointer<DatabaseManager> db(makeDb());

    const int custom = db->addGroup(QString::fromUtf8("阅读学习"));
    assert(custom > 0);
    // 同名组别返回既有 id，不重复插入。
    assert(db->addGroup(QString::fromUtf8("阅读学习")) == custom);
    assert(db->addGroup(QStringLiteral("   ")) < 0);

    db->renameGroup(custom, QString::fromUtf8("深度阅读"));
    bool found = false;
    for (const QVariantMap &row : db->getGroups()) {
        if (row["id"].toInt() == custom) {
            assert(row["name"].toString() == QString::fromUtf8("深度阅读"));
            assert(!row["builtin"].toBool());
            found = true;
        }
    }
    assert(found);

    db->removeGroup(custom);
    for (const QVariantMap &row : db->getGroups())
        assert(row["id"].toInt() != custom);

    std::cout << "test_group_crud PASS" << std::endl;
}

void test_group_icons()
{
    QString path;
    QScopedPointer<DatabaseManager> db(makeDb(&path));

    // 新建组别可指定 emoji 图标。
    const int fitness = db->addGroup(QString::fromUtf8("运动健身"), QString::fromUtf8("🏃"));
    assert(fitness > 0);
    bool found = false;
    for (const QVariantMap &row : db->getGroups()) {
        if (row["id"].toInt() == fitness) {
            assert(row["icon"].toString() == QString::fromUtf8("🏃"));
            found = true;
        }
    }
    assert(found);

    // 不传图标时按名称自动分配非空回退图标（跨进程稳定）。
    const int reading = db->addGroup(QString::fromUtf8("阅读学习"));
    QString readingIcon;
    for (const QVariantMap &row : db->getGroups())
        if (row["id"].toInt() == reading)
            readingIcon = row["icon"].toString();
    assert(readingIcon == GroupIcons::fallbackIcon(QString::fromUtf8("阅读学习")));
    assert(!readingIcon.isEmpty());

    // 图标可单独更换。
    db->setGroupIcon(fitness, QString::fromUtf8("🎯"));
    for (const QVariantMap &row : db->getGroups())
        if (row["id"].toInt() == fitness)
            assert(row["icon"].toString() == QString::fromUtf8("🎯"));

    // 置空行重开数据库后由迁移按名称回填；非空图标不受影响。
    db->setGroupIcon(fitness, QString());
    db->close();
    DatabaseManager reopened(path);
    for (const QVariantMap &row : reopened.getGroups()) {
        assert(!row["icon"].toString().isEmpty());
        if (row["id"].toInt() == reading)
            assert(row["icon"].toString() == readingIcon);
    }

    std::cout << "test_group_icons PASS" << std::endl;
}

void test_group_membership_and_ungrouped_fallback()
{
    QScopedPointer<DatabaseManager> db(makeDb());

    int devGroup = -1;
    for (const QVariantMap &row : db->getGroups()) {
        if (row["name"].toString() == QString::fromUtf8("开发效率"))
            devGroup = row["id"].toInt();
    }
    assert(devGroup > 0);

    addSession(db.data(), kCode, QStringLiteral("VS Code"), 300);
    addSession(db.data(), kChrome, QStringLiteral("Chrome"), 200);
    addSession(db.data(), kWechat, QStringLiteral("WeChat"), 100);

    // 尚未把任何应用归入组别时不应产出组别维度：整份数据只会坍缩成一个
    // 「未分组」桶，与应用排行完全重复。
    assert(db->getGroupRank().isEmpty());

    db->setAppGroup(QStringLiteral("code.exe"), devGroup);
    QVector<QVariantMap> split = db->getGroupRank();
    assert(split.size() == 2);
    assert(split[0]["group_name"].toString() == QString::fromUtf8("开发效率"));
    assert(split[0]["total_seconds"].toInt() == 300);
    assert(split[1]["group_name"].toString() == QString::fromUtf8("未分组"));
    assert(split[1]["total_seconds"].toInt() == 300);

    // 移出组别后回落「未分组」。
    // 移出后仍有其它成员留在原组别：被移出的应用应汇入「未分组」桶，
    // 且各桶之和等于当日总时长。
    db->setAppGroup(QStringLiteral("chrome.exe"), devGroup);
    db->setAppGroup(QStringLiteral("code.exe"), -1);
    QVector<QVariantMap> back = db->getGroupRank();
    assert(back.size() == 2);
    int sum = 0;
    for (const QVariantMap &row : back) {
        sum += row["total_seconds"].toInt();
        const QString name = row["group_name"].toString();
        assert(name == QString::fromUtf8("开发效率") || name == QString::fromUtf8("未分组"));
    }
    assert(sum == 600);

    std::cout << "test_group_membership_and_ungrouped_fallback PASS" << std::endl;
}

void test_removing_group_clears_members()
{
    QScopedPointer<DatabaseManager> db(makeDb());

    int devGroup = -1;
    for (const QVariantMap &row : db->getGroups()) {
        if (row["name"].toString() == QString::fromUtf8("开发效率"))
            devGroup = row["id"].toInt();
    }
    assert(devGroup > 0);

    addSession(db.data(), kCode, QStringLiteral("VS Code"), 300);
    addSession(db.data(), kChrome, QStringLiteral("Chrome"), 200);
    db->setAppGroup(QStringLiteral("code.exe"), devGroup);
    QVector<QVariantMap> grouped = db->getGroupRank();

    assert(grouped.size() == 2);
    assert(grouped[0]["group_name"].toString() == QString::fromUtf8("开发效率"));
    assert(grouped[0]["total_seconds"].toInt() == 300);

    // 删除组别必须连带清空成员，否则会留下指向已删除组别的孤儿行。
    db->removeGroup(devGroup);
    assert(db->getAppGroupMembers().isEmpty());
    assert(db->getGroupRank().isEmpty());

    std::cout << "test_removing_group_clears_members PASS" << std::endl;
}

void test_merge_accumulates_and_is_reversible()
{
    QScopedPointer<DatabaseManager> db(makeDb());

    addSession(db.data(), kCode, QStringLiteral("VS Code"), 300);
    addSession(db.data(), "C:\\Users\\me\\AppData\\Code.exe", QStringLiteral("VS Code"), 120);
    addSession(db.data(), kChrome, QStringLiteral("Chrome"), 200);

    QVector<QVariantMap> before = db->getAppRank();
    int codeTotal = 0;
    for (const QVariantMap &row : before)
        if (row["app_name"].toString() == QStringLiteral("VS Code"))
            codeTotal += row["total_seconds"].toInt();
    assert(codeTotal == 420);

    assert(db->setAppMerge(QStringLiteral("chrome.exe"), QStringLiteral("code.exe")));

    QVector<AppEntry> merged = db->getManagedApps();
    int codeEntries = 0;
    for (const AppEntry &entry : merged) {
        if (entry.processKey == QStringLiteral("code.exe")) {
            ++codeEntries;
            // 合并后 Chrome 的时长并入 VS Code，且原始行未被改写。
            assert(entry.totalSeconds == 620);
        }
        if (entry.processKey == QStringLiteral("chrome.exe"))
            assert(entry.mergedInto == QStringLiteral("code.exe"));
    }
    assert(codeEntries == 1);

    // 解除合并应完全复原。
    db->removeAppMerge(QStringLiteral("chrome.exe"));
    QVector<QVariantMap> unmerged = db->getAppRank();
    int restored = 0;
    for (const QVariantMap &row : unmerged)
        if (row["app_name"].toString() == QStringLiteral("VS Code"))
            restored += row["total_seconds"].toInt();
    assert(restored == 420);

    std::cout << "test_merge_accumulates_and_is_reversible PASS" << std::endl;
}

void test_merge_rejects_invalid_targets()
{
    QScopedPointer<DatabaseManager> db(makeDb());

    addSession(db.data(), kCode, QStringLiteral("VS Code"), 300);
    addSession(db.data(), kChrome, QStringLiteral("Chrome"), 200);
    addSession(db.data(), kWechat, QStringLiteral("WeChat"), 100);
    addSession(db.data(), kSteam, QStringLiteral("Steam"), 50);

    // 自身、空键、不存在的目标。
    assert(!db->setAppMerge(QStringLiteral("code.exe"), QStringLiteral("code.exe")));
    assert(!db->setAppMerge(QStringLiteral("code.exe"), QString()));
    assert(!db->setAppMerge(QString(), QStringLiteral("code.exe")));

    assert(db->setAppMerge(QStringLiteral("chrome.exe"), QStringLiteral("code.exe")));
    // 目标本身已是源时改指向其根，保持链深为 1，不形成环。
    assert(db->setAppMerge(QStringLiteral("wechat.exe"), QStringLiteral("chrome.exe")));
    QMap<QString, QString> merges = db->getAppMerges();
    assert(merges.value(QStringLiteral("wechat.exe")) == QStringLiteral("code.exe"));
    // 反向合并会成环，必须拒绝。
    assert(!db->setAppMerge(QStringLiteral("code.exe"), QStringLiteral("wechat.exe")));

    QVector<AppEntry> apps = db->getManagedApps();
    int roots = 0;
    for (const AppEntry &entry : apps)
        if (entry.mergedInto.isEmpty())
            ++roots;
    // code.exe 与 steam.exe 两个根，chrome/wechat 均已并入。
    assert(roots == 2);

    std::cout << "test_merge_rejects_invalid_targets PASS" << std::endl;
}

void test_ignored_affects_group_rank()
{
    QScopedPointer<DatabaseManager> db(makeDb());

    int devGroup = -1;
    for (const QVariantMap &row : db->getGroups()) {
        if (row["name"].toString() == QString::fromUtf8("开发效率"))
            devGroup = row["id"].toInt();
    }
    assert(devGroup > 0);

    addSession(db.data(), kCode, QStringLiteral("VS Code"), 300);
    addSession(db.data(), kChrome, QStringLiteral("Chrome"), 200);

    db->setAppGroup(QStringLiteral("code.exe"), devGroup);

    db->addIgnoredApp(kCode);
    assert(db->isAppIgnored(QStringLiteral("code.exe")));
    assert(!db->isAppIgnored(QStringLiteral("chrome.exe")));

    QVector<QVariantMap> filtered = db->getGroupRank();
    assert(filtered.size() == 1);
    assert(filtered[0]["group_name"].toString() == QString::fromUtf8("未分组"));
    assert(filtered[0]["total_seconds"].toInt() == 200);

    // 被屏蔽的应用仍要出现在管理界面，否则用户无从解除屏蔽。
    bool present = false;
    for (const AppEntry &entry : db->getManagedApps()) {
        if (entry.processKey != QStringLiteral("code.exe"))
            continue;
        present = true;
        assert(entry.ignored);
    }
    assert(present);

    const QMap<int, QString> ignored = db->getIgnoredApps();
    assert(ignored.size() == 1);
    db->removeIgnoredApp(ignored.firstKey());
    assert(!db->isAppIgnored(QStringLiteral("code.exe")));
    assert(db->getGroupRank().size() == 2);

    std::cout << "test_ignored_affects_group_rank PASS" << std::endl;
}

void test_managed_apps_reports_metadata()
{
    QScopedPointer<DatabaseManager> db(makeDb());

    addSession(db.data(), kCode, QStringLiteral("VS Code"), 300);
    addSession(db.data(), kCode, QStringLiteral("VS Code"), 120);
    addSession(db.data(), kChrome, QStringLiteral("Chrome"), 600);

    int devGroup = -1;
    for (const QVariantMap &row : db->getGroups()) {
        if (row["name"].toString() == QString::fromUtf8("开发效率"))
            devGroup = row["id"].toInt();
    }
    db->setAppGroup(QStringLiteral("code.exe"), devGroup);
    db->setAppAlias(kCode, QStringLiteral("Visual Studio Code"));

    QVector<AppEntry> apps = db->getManagedApps();
    assert(apps.size() == 2);
    // 按累计时长降序：Chrome 600s > code 420s。
    assert(apps[0].processKey == QStringLiteral("chrome.exe"));
    assert(apps[0].totalSeconds == 600);
    assert(apps[1].processKey == QStringLiteral("code.exe"));
    // 别名优先于会话里固化的 app_name。
    assert(apps[1].displayName == QStringLiteral("Visual Studio Code"));
    assert(apps[1].groupId == devGroup);
    assert(apps[1].mergedInto.isEmpty());
    assert(apps[1].totalSeconds == 420);
    assert(apps[1].sessionCount == 2);
    assert(!apps[1].ignored);

    std::cout << "test_managed_apps_reports_metadata PASS" << std::endl;
}

void test_recent_window_titles()
{
    QScopedPointer<DatabaseManager> db(makeDb());
    const QDateTime base = QDateTime::currentDateTime().addSecs(-3600);
    auto add = [&](const QString &process, const QString &title, int offsetSecs) {
        const QDateTime start = base.addSecs(offsetSecs);
        db->insertSession(process, title, QStringLiteral("app"),
                          start, start.addSecs(60), 60);
    };
    add(QStringLiteral("pid_12345"), QStringLiteral("OBS Studio"), 0);
    add(QStringLiteral("pid_12345"), QStringLiteral("OBS Studio"), 600); // 重复标题去重
    add(QStringLiteral("pid_12345"), QString(), 900);                    // 空标题不参与
    add(QStringLiteral("pid_12345"), QStringLiteral("Premiere Pro"), 1200);
    add(QStringLiteral("code.exe"), QStringLiteral("window"), 0);

    const QStringList titles = db->getRecentWindowTitles(QStringLiteral("pid_12345"), 10);
    assert(titles.size() == 2);
    assert(titles[0] == QStringLiteral("Premiere Pro")); // 按最近出现时间降序
    assert(titles[1] == QStringLiteral("OBS Studio"));
    assert(db->getRecentWindowTitles(QStringLiteral("chrome.exe"), 10).isEmpty());
    // limit 生效
    assert(db->getRecentWindowTitles(QStringLiteral("pid_12345"), 1).size() == 1);

    std::cout << "test_recent_window_titles PASS" << std::endl;
}

void test_group_rank_respects_min_record_threshold()
{
    QScopedPointer<DatabaseManager> db(makeDb());

    // 默认阈值 40s：低于阈值的会话不计入任何组别。
    addSession(db.data(), kCode, QStringLiteral("VS Code"), 30);
    addSession(db.data(), kChrome, QStringLiteral("Chrome"), 100);

    int devGroup = -1;
    for (const QVariantMap &row : db->getGroups()) {
        if (row.value(QStringLiteral("name")).toString() == QString::fromUtf8("开发效率"))
            devGroup = row.value(QStringLiteral("id")).toInt();
    }
    assert(devGroup > 0);
    db->setAppGroup(QStringLiteral("chrome.exe"), devGroup);

    // 阈值过滤后只剩 Chrome 有有效时长，30s 的 VS Code 不应出现在任何组别。
    QVector<QVariantMap> rank = db->getGroupRank();
    assert(rank.size() == 1);
    assert(rank[0]["group_name"].toString() == QString::fromUtf8("开发效率"));
    assert(rank[0]["total_seconds"].toInt() == 100);

    // 管理界面刻意不受阈值影响：极短会话的应用也要可见。
    QVector<AppEntry> apps = db->getManagedApps();
    assert(apps.size() == 2);

    std::cout << "test_group_rank_respects_min_record_threshold PASS" << std::endl;
}

void test_group_rank_range_query()
{
    QScopedPointer<DatabaseManager> db(makeDb());

    const QDate today = QDate::currentDate();
    const QDate yesterday = today.addDays(-1);
    const QDateTime morning(QDateTime(today, QTime(9, 0)));
    const QDateTime yesterdayNoon(QDateTime(yesterday, QTime(12, 0)));

    db->insertSession(kCode, QStringLiteral("win"), QStringLiteral("VS Code"),
                      morning, morning, 300);
    db->insertSession(kCode, QStringLiteral("win"), QStringLiteral("VS Code"),
                      yesterdayNoon, yesterdayNoon, 500);

    int devGroup = -1;
    for (const QVariantMap &row : db->getGroups()) {
        if (row.value(QStringLiteral("name")).toString() == QString::fromUtf8("开发效率"))
            devGroup = row.value(QStringLiteral("id")).toInt();
    }
    assert(devGroup > 0);
    db->setAppGroup(QStringLiteral("code.exe"), devGroup);

    const QVector<QVariantMap> single =
        db->getGroupRank(today.toString(Qt::ISODate), today.toString(Qt::ISODate));
    assert(single.size() == 1);
    assert(single[0]["total_seconds"].toInt() == 300);

    const QVector<QVariantMap> range =
        db->getGroupRank(yesterday.toString(Qt::ISODate), today.toString(Qt::ISODate));
    assert(range.size() == 1);
    assert(range[0]["total_seconds"].toInt() == 800);

    std::cout << "test_group_rank_range_query PASS" << std::endl;
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    test_preset_groups_seeded_once();
    test_group_crud();
    test_group_icons();
    test_group_membership_and_ungrouped_fallback();
    test_removing_group_clears_members();
    test_merge_accumulates_and_is_reversible();
    test_merge_rejects_invalid_targets();
    test_ignored_affects_group_rank();
    test_managed_apps_reports_metadata();
    test_recent_window_titles();
    test_group_rank_respects_min_record_threshold();
    test_group_rank_range_query();

    std::cout << "All app management tests passed!" << std::endl;
    return 0;
}
