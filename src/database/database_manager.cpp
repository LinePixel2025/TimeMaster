#include "database_manager.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDir>
#include <QStandardPaths>
#include <QUuid>
#include <algorithm>
#include "utility/process_identity.h"

namespace {

// 同一应用可能被记成多种 app_name 变体:历史兜底名(可执行文件名)、版本资源
// FileDescription,两者除大小写外还常差空格,如 "TimeMaster"/"Time Master"。
// SQLite 的 GROUP BY 对 app_name 区分大小写,无法在 SQL 层合并,故在 C++ 侧按
// "小写+去空格" 归一化应用名合并,显示名(及 process_name)保留组内累计时长
// 最大的变体。getDailySummaries 还带日期维度 d,合并键需包含日期。
QString normalizedAppNameKey(const QString &name)
{
    const QString lower = name.toLower();
    QString key;
    key.reserve(lower.size());
    for (const QChar c : lower) {
        if (!c.isSpace())
            key.append(c);
    }
    return key;
}

void mergeAppNameVariants(QVector<QVariantMap> &rows)
{
    QVector<QVariantMap> merged;
    QHash<QString, int> index;        // d + 归一化应用名 -> merged 中的下标
    QHash<QString, int> bestSeconds;  // 同上键 -> 组内单行最大时长,用于选规范变体
    index.reserve(rows.size());
    for (const QVariantMap &row : rows) {
        const QString appName = row.value(QStringLiteral("app_name")).toString();
        QString key = normalizedAppNameKey(appName);
        const QString day = row.value(QStringLiteral("d")).toString();
        if (!day.isEmpty())
            key = day + QLatin1Char('\x1f') + key;

        const int seconds = row.value(QStringLiteral("total_seconds")).toInt();
        const auto it = index.constFind(key);
        if (it == index.cend()) {
            index.insert(key, merged.size());
            bestSeconds.insert(key, seconds);
            merged.append(row);
            continue;
        }
        QVariantMap &target = merged[it.value()];
        target[QStringLiteral("total_seconds")] =
            target.value(QStringLiteral("total_seconds")).toInt() + seconds;
        if (seconds > bestSeconds.value(key)) {
            bestSeconds.insert(key, seconds);
            target[QStringLiteral("app_name")] = appName;
            if (row.contains(QStringLiteral("process_name")))
                target[QStringLiteral("process_name")] = row.value(QStringLiteral("process_name"));
        }
    }
    rows.swap(merged);
}

// start_time 以定长前缀 ISO 文本存储(yyyy-MM-ddTHH:mm:ss[.zzz]),字典序
// 与时间序一致,故 `start_time >= ? AND start_time < 次日` 的开区间与
// `date(start_time) BETWEEN ...` 语义等价(混合毫秒后缀不影响边界比较),
// 且能命中 idx_sessions_start。参数传入纯日期串("yyyy-MM-dd"),
// 其字典序天然小于当天所有 "yyyy-MM-ddT..." 值,可直接作下界。
QString dayEndExclusive(const QString &isoDate)
{
    const QDate end = QDate::fromString(isoDate, Qt::ISODate);
    if (end.isValid())
        return end.addDays(1).toString(Qt::ISODate);
    // 非法日期兜底:附加一个大于任何可打印字符的界,不放大为全表
    return isoDate + QLatin1Char('\x7f');
}

QString dayEndExclusive(const QDate &date)
{
    return date.addDays(1).toString(Qt::ISODate);
}

/// 进程路径 -> 可读名（去目录与 .exe 后缀），管理界面无会话记录时的兜底显示。
QString friendlyProcessName(const QString &processPath)
{
    int pos = processPath.lastIndexOf(QLatin1Char('\\'));
    QString name = (pos >= 0) ? processPath.mid(pos + 1) : processPath;
    const int slash = name.lastIndexOf(QLatin1Char('/'));
    if (slash >= 0)
        name = name.mid(slash + 1);
    if (name.endsWith(QLatin1String(".exe"), Qt::CaseInsensitive))
        name.chop(4);
    if (!name.isEmpty())
        name[0] = name[0].toUpper();
    return name;
}

} // namespace

DatabaseManager::DatabaseManager(const QString &dbPath)
    : m_dbPath(dbPath)
{
    if (m_dbPath.isEmpty()) {
        QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir().mkpath(appData);
        m_dbPath = appData + "/data.db";
    }

    m_db = QSqlDatabase::addDatabase("QSQLITE", QUuid::createUuid().toString());
    m_db.setDatabaseName(m_dbPath);
    if (!m_db.open())
        qFatal("Failed to open database: %s", qPrintable(m_db.lastError().text()));
    QSqlQuery pragma(m_db);
    pragma.exec("PRAGMA busy_timeout = 5000");
    // WAL:追踪线程(独立连接)的写事务不再以回滚日志独占阻塞 GUI 线程的
    // 统计读(现状最坏等满 busy_timeout 5s 表现为界面卡顿)。journal_mode
    // 是库文件属性,多连接重复设置幂等;synchronous=NORMAL 配合 WAL 仅在
    // 检查点落盘,崩溃最多丢最后一个已提交事务。
    pragma.exec("PRAGMA journal_mode = WAL");
    pragma.exec("PRAGMA synchronous = NORMAL");
    migrate();
}

DatabaseManager::~DatabaseManager()
{
    close();
}

void DatabaseManager::migrate()
{
    QSqlQuery q(m_db);
    if (!m_db.transaction())
        qWarning() << "Failed to start database migration transaction:" << m_db.lastError();
    q.exec(
        "CREATE TABLE IF NOT EXISTS sessions ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "process_name TEXT NOT NULL, "
        "window_title TEXT NOT NULL DEFAULT '', "
        "app_name TEXT NOT NULL, "
        "start_time TEXT NOT NULL, "
        "end_time TEXT, "
        "duration_seconds INTEGER DEFAULT 0)"
    );

    bool hasProcessKey = false;
    q.exec("PRAGMA table_info(sessions)");
    while (q.next()) {
        if (q.value("name").toString() == "process_key") {
            hasProcessKey = true;
            break;
        }
    }
    if (!hasProcessKey)
        q.exec("ALTER TABLE sessions ADD COLUMN process_key TEXT NOT NULL DEFAULT ''");

    q.exec("SELECT id, process_name FROM sessions WHERE process_key = '' OR process_key IS NULL");
    QVector<QPair<qint64, QString>> missingKeys;
    while (q.next())
        missingKeys.append({q.value(0).toLongLong(), ProcessIdentity::normalizeKey(q.value(1).toString())});
    QSqlQuery updateKey(m_db);
    updateKey.prepare("UPDATE sessions SET process_key = ? WHERE id = ?");
    for (const auto &entry : missingKeys) {
        updateKey.bindValue(0, entry.second);
        updateKey.bindValue(1, entry.first);
        if (!updateKey.exec())
            qWarning() << "Failed to backfill process_key:" << updateKey.lastError();
    }

    q.exec("CREATE INDEX IF NOT EXISTS idx_sessions_start ON sessions(start_time)");
    q.exec("CREATE INDEX IF NOT EXISTS idx_sessions_app ON sessions(app_name)");
    q.exec("CREATE INDEX IF NOT EXISTS idx_sessions_process_key ON sessions(process_key)");

    q.exec(
        "CREATE TABLE IF NOT EXISTS settings ("
        "key TEXT PRIMARY KEY, "
        "value TEXT NOT NULL)"
    );

    q.exec(
        "CREATE TABLE IF NOT EXISTS ignored_apps ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "process_name TEXT NOT NULL UNIQUE)"
    );

    q.exec(
        "CREATE TABLE IF NOT EXISTS app_aliases ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "process_name TEXT NOT NULL UNIQUE, "
        "display_name TEXT NOT NULL)"
    );

    q.exec(
        "CREATE TABLE IF NOT EXISTS app_groups ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "name TEXT NOT NULL UNIQUE, "
        "sort_order INTEGER NOT NULL DEFAULT 0, "
        "builtin INTEGER NOT NULL DEFAULT 0)"
    );

    q.exec(
        "CREATE TABLE IF NOT EXISTS app_group_members ("
        "process_key TEXT PRIMARY KEY, "
        "group_id INTEGER NOT NULL)"
    );

    // 合并映射：源进程键的时长在统计时算到 target_key 头上，sessions 原始行
    // 不改写，解除映射即可还原（app_merges 只表达『谁并入了谁』）。
    q.exec(
        "CREATE TABLE IF NOT EXISTS app_merges ("
        "process_key TEXT PRIMARY KEY, "
        "target_key TEXT NOT NULL)"
    );

    // 预设组别只播种一次：哨兵键先于插入写入，因此用户删掉某个预设后重启
    // 不会被重新塞回（注意别改成先播种后写哨兵）。
    q.exec("SELECT value FROM settings WHERE key = 'app_groups_seeded'");
    const bool groupsSeeded = q.next();
    if (!groupsSeeded) {
        const char *presetNames[] = {
            "\xe5\xbc\x80\xe5\x8f\x91\xe6\x95\x88\xe7\x8e\x87",   // 开发效率
            "\xe8\xae\xbe\xe8\xae\xa1\xe5\x88\x9b\xe4\xbd\x9c",   // 设计创作
            "\xe7\xa4\xbe\xe4\xba\xa4\xe9\x80\x9a\xe8\xae\xaf",   // 社交通讯
            "\xe5\xbd\xb1\xe9\x9f\xb3\xe5\xa8\xb1\xe4\xb9\x90",   // 影音娱乐
            "\xe5\x8a\x9e\xe5\x85\xac\xe6\x96\x87\xe6\xa1\xa3",   // 办公文档
            "\xe6\xb8\xb8\xe6\x88\x8f"                            // 游戏
        };
        QSqlQuery insertGroup(m_db);
        insertGroup.prepare(
            "INSERT OR IGNORE INTO app_groups (name, sort_order, builtin) "
            "VALUES (?, ?, 1)");
        for (int i = 0; i < int(sizeof(presetNames) / sizeof(presetNames[0])); ++i) {
            insertGroup.bindValue(0, QString::fromUtf8(presetNames[i]));
            insertGroup.bindValue(1, i);
            if (!insertGroup.exec())
                qWarning() << "Failed to seed preset group:" << insertGroup.lastError();
        }
        q.exec("INSERT OR IGNORE INTO settings (key, value) VALUES ('app_groups_seeded', '1')");
    }

    q.exec("INSERT OR IGNORE INTO settings (key, value) VALUES ('tracking_enabled', 'true')");
    q.exec("INSERT OR IGNORE INTO settings (key, value) VALUES ('poll_interval', '1')");
    q.exec("INSERT OR IGNORE INTO settings (key, value) VALUES ('idle_threshold', '60')");
    q.exec("INSERT OR IGNORE INTO settings (key, value) VALUES ('auto_start', 'false')");

    q.exec("SELECT process_name FROM ignored_apps ORDER BY id ASC");
    QSet<QString> ignoredKeys;
    while (q.next())
        ignoredKeys.insert(ProcessIdentity::normalizeKey(q.value(0).toString()));
    q.exec("DELETE FROM ignored_apps");
    QSqlQuery insertIgnored(m_db);
    insertIgnored.prepare("INSERT OR IGNORE INTO ignored_apps (process_name) VALUES (?)");
    for (const QString &key : ignoredKeys) {
        if (key.isEmpty())
            continue;
        insertIgnored.bindValue(0, key);
        insertIgnored.exec();
    }

    q.exec("SELECT process_name, display_name FROM app_aliases ORDER BY id ASC");
    QMap<QString, QString> aliases;
    while (q.next())
        aliases[ProcessIdentity::normalizeKey(q.value(0).toString())] = q.value(1).toString();
    q.exec("DELETE FROM app_aliases");
    QSqlQuery insertAlias(m_db);
    insertAlias.prepare("INSERT OR REPLACE INTO app_aliases (process_name, display_name) VALUES (?, ?)");
    for (auto it = aliases.cbegin(); it != aliases.cend(); ++it) {
        if (it.key().isEmpty())
            continue;
        insertAlias.bindValue(0, it.key());
        insertAlias.bindValue(1, it.value());
        insertAlias.exec();
    }

    if (!m_db.commit())
        qWarning() << "Failed to commit database migration:" << m_db.lastError();
}

qint64 DatabaseManager::insertSession(const QString &processName, const QString &windowTitle,
                                       const QString &appName, const QDateTime &startTime,
                                       const QDateTime &endTime, int durationSeconds)
{
    QMutexLocker lock(&m_mutex);
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO sessions (process_name, process_key, window_title, app_name, start_time, end_time, duration_seconds) "
              "VALUES (?, ?, ?, ?, ?, ?, ?)");
    q.addBindValue(processName);
    q.addBindValue(ProcessIdentity::normalizeKey(processName));
    q.addBindValue(windowTitle);
    q.addBindValue(appName);
    q.addBindValue(startTime.toString(Qt::ISODate));
    q.addBindValue(endTime.isValid() ? endTime.toString(Qt::ISODate) : QVariant());
    q.addBindValue(durationSeconds);
    if (!q.exec()) {
        qWarning() << "insertSession failed:" << q.lastError();
        return -1;
    }
    return q.lastInsertId().toLongLong();
}

bool DatabaseManager::updateSessionEnd(qint64 sessionId, const QDateTime &endTime, int durationSeconds)
{
    QMutexLocker lock(&m_mutex);
    QSqlQuery q(m_db);
    q.prepare("UPDATE sessions SET end_time=?, duration_seconds=? WHERE id=?");
    q.addBindValue(endTime.toString(Qt::ISODate));
    q.addBindValue(durationSeconds);
    q.addBindValue(sessionId);
    if (!q.exec()) {
        qWarning() << "updateSessionEnd failed:" << q.lastError();
        return false;
    }
    return q.numRowsAffected() > 0;
}

bool DatabaseManager::updateSessionDuration(qint64 sessionId, int durationSeconds)
{
    QMutexLocker lock(&m_mutex);
    QSqlQuery q(m_db);
    q.prepare("UPDATE sessions SET duration_seconds=? WHERE id=?");
    q.addBindValue(durationSeconds);
    q.addBindValue(sessionId);
    if (!q.exec()) {
        qWarning() << "updateSessionDuration failed:" << q.lastError();
        return false;
    }
    return q.numRowsAffected() > 0;
}

QString DatabaseManager::databasePath() const
{
    return m_dbPath;
}

QVector<QVariantMap> DatabaseManager::getTodaySummary()
{
    int threshold = getSetting("min_record_threshold", "40").toInt();
    QMutexLocker lock(&m_mutex);

    QSqlQuery q(m_db);
    const QString dayStart = QDate::currentDate().toString(Qt::ISODate);
    q.prepare("SELECT app_name, SUM(duration_seconds) as total_seconds "
              "FROM sessions WHERE start_time >= ? AND start_time < ? "
              "AND duration_seconds >= ? "
              "AND NOT EXISTS (SELECT 1 FROM ignored_apps ia "
              "WHERE sessions.process_key = ia.process_name) "
              "GROUP BY app_name ORDER BY total_seconds DESC");
    q.addBindValue(dayStart);
    q.addBindValue(dayEndExclusive(dayStart));
    q.addBindValue(threshold);
    q.exec();
    QVector<QVariantMap> results;
    while (q.next()) {
        QVariantMap row;
        row["app_name"] = q.value("app_name");
        row["total_seconds"] = q.value("total_seconds");
        results.append(row);
    }
    mergeAppNameVariants(results);
    std::sort(results.begin(), results.end(),
              [](const QVariantMap &a, const QVariantMap &b) {
                  return a.value(QStringLiteral("total_seconds")).toInt() >
                         b.value(QStringLiteral("total_seconds")).toInt();
              });
    return results;
}

int DatabaseManager::getTodayTotal()
{
    int threshold = getSetting("min_record_threshold", "40").toInt();
    QMutexLocker lock(&m_mutex);

    QSqlQuery q(m_db);
    q.prepare("SELECT COALESCE(SUM(duration_seconds), 0) as total "
              "FROM sessions WHERE start_time >= ? AND start_time < ? "
              "AND duration_seconds >= ? "
              "AND NOT EXISTS (SELECT 1 FROM ignored_apps ia "
              "WHERE sessions.process_key = ia.process_name)");
    QString todayStr = QDate::currentDate().toString(Qt::ISODate);
    q.addBindValue(todayStr);
    q.addBindValue(dayEndExclusive(todayStr));
    q.addBindValue(threshold);
    q.exec();
    if (q.next())
        return q.value("total").toInt();
    return 0;
}

int DatabaseManager::getYesterdayTotal()
{
    int threshold = getSetting("min_record_threshold", "40").toInt();
    QMutexLocker lock(&m_mutex);

    QSqlQuery q(m_db);
    q.prepare("SELECT COALESCE(SUM(duration_seconds), 0) as total "
              "FROM sessions WHERE start_time >= ? AND start_time < ? "
              "AND duration_seconds >= ? "
              "AND NOT EXISTS (SELECT 1 FROM ignored_apps ia "
              "WHERE sessions.process_key = ia.process_name)");
    const QDate yesterday = QDate::currentDate().addDays(-1);
    q.addBindValue(yesterday.toString(Qt::ISODate));
    q.addBindValue(dayEndExclusive(yesterday));
    q.addBindValue(threshold);
    q.exec();
    if (q.next())
        return q.value("total").toInt();
    return 0;
}

QVector<QVariantMap> DatabaseManager::getWeekSummary()
{
    int threshold = getSetting("min_record_threshold", "40").toInt();
    QMutexLocker lock(&m_mutex);
    QDate today = QDate::currentDate();
    QDate monday = today.addDays(-today.dayOfWeek() + 1);

    QSqlQuery q(m_db);
    q.prepare("SELECT substr(start_time, 1, 10) as d, SUM(duration_seconds) as total_seconds "
              "FROM sessions WHERE start_time >= ? AND start_time < ? "
              "AND duration_seconds >= ? "
              "AND NOT EXISTS (SELECT 1 FROM ignored_apps ia "
              "WHERE sessions.process_key = ia.process_name) "
              "GROUP BY d ORDER BY d ASC");
    q.addBindValue(monday.toString(Qt::ISODate));
    q.addBindValue(dayEndExclusive(today));
    q.addBindValue(threshold);
    q.exec();
    QVector<QVariantMap> results;
    while (q.next()) {
        QVariantMap row;
        row["d"] = q.value("d");
        row["total_seconds"] = q.value("total_seconds");
        results.append(row);
    }
    return results;
}

QVector<QVariantMap> DatabaseManager::getMonthSummary()
{
    int threshold = getSetting("min_record_threshold", "40").toInt();
    QMutexLocker lock(&m_mutex);
    QDate today = QDate::currentDate();
    QDate monthStart(today.year(), today.month(), 1);

    QSqlQuery q(m_db);
    q.prepare("SELECT substr(start_time, 1, 10) as d, SUM(duration_seconds) as total_seconds "
              "FROM sessions WHERE start_time >= ? AND start_time < ? "
              "AND duration_seconds >= ? "
              "AND NOT EXISTS (SELECT 1 FROM ignored_apps ia "
              "WHERE sessions.process_key = ia.process_name) "
              "GROUP BY d ORDER BY d ASC");
    q.addBindValue(monthStart.toString(Qt::ISODate));
    q.addBindValue(dayEndExclusive(today));
    q.addBindValue(threshold);
    q.exec();
    QVector<QVariantMap> results;
    while (q.next()) {
        QVariantMap row;
        row["d"] = q.value("d");
        row["total_seconds"] = q.value("total_seconds");
        results.append(row);
    }
    return results;
}

QVector<QVariantMap> DatabaseManager::getAppRank(const QDate &targetDate)
{
    int threshold = getSetting("min_record_threshold", "40").toInt();
    QMutexLocker lock(&m_mutex);

    QSqlQuery q(m_db);
    q.prepare(
        "SELECT app_name, process_name, SUM(duration_seconds) as total_seconds "
        "FROM sessions WHERE start_time >= ? AND start_time < ? "
        "AND duration_seconds >= ? "
        "AND NOT EXISTS (SELECT 1 FROM ignored_apps ia "
        "WHERE sessions.process_key = ia.process_name) "
        "GROUP BY app_name, process_name");
    q.addBindValue(targetDate.toString(Qt::ISODate));
    q.addBindValue(dayEndExclusive(targetDate));
    q.addBindValue(threshold);
    q.exec();
    QVector<QVariantMap> results;
    while (q.next()) {
        QVariantMap row;
        row["app_name"] = q.value("app_name");
        row["process_name"] = q.value("process_name");
        row["total_seconds"] = q.value("total_seconds");
        results.append(row);
    }
    mergeAppNameVariants(results);
    std::sort(results.begin(), results.end(),
              [](const QVariantMap &a, const QVariantMap &b) {
                  return a.value(QStringLiteral("total_seconds")).toInt() >
                         b.value(QStringLiteral("total_seconds")).toInt();
              });
    return results;
}

QVector<QVariantMap> DatabaseManager::getAllSessions(const QString &startDate, const QString &endDate)
{
    int threshold = getSetting("min_record_threshold", "40").toInt();
    QMutexLocker lock(&m_mutex);

    QSqlQuery q(m_db);
    if (!startDate.isEmpty() && !endDate.isEmpty()) {
        q.prepare("SELECT * FROM sessions WHERE start_time >= ? AND start_time < ? "
                  "AND duration_seconds >= ? "
                  "AND NOT EXISTS (SELECT 1 FROM ignored_apps ia "
                  "WHERE sessions.process_key = ia.process_name) "
                  "ORDER BY start_time ASC");
        q.addBindValue(startDate);
        q.addBindValue(dayEndExclusive(endDate));
        q.addBindValue(threshold);
    } else {
        q.prepare("SELECT * FROM sessions "
                  "WHERE duration_seconds >= ? "
                  "AND NOT EXISTS (SELECT 1 FROM ignored_apps ia "
                  "WHERE sessions.process_key = ia.process_name) "
                  "ORDER BY start_time ASC");
        q.addBindValue(threshold);
    }
    q.exec();
    QVector<QVariantMap> results;
    while (q.next()) {
        QVariantMap row;
        row["id"] = q.value("id");
        row["process_name"] = q.value("process_name");
        row["window_title"] = q.value("window_title");
        row["app_name"] = q.value("app_name");
        row["start_time"] = q.value("start_time");
        row["end_time"] = q.value("end_time");
        row["duration_seconds"] = q.value("duration_seconds");
        results.append(row);
    }
    return results;
}

QVector<QVariantMap> DatabaseManager::getDailySummaries(const QString &startDate, const QString &endDate)
{
    int threshold = getSetting("min_record_threshold", "40").toInt();
    QMutexLocker lock(&m_mutex);

    QSqlQuery q(m_db);
    if (!startDate.isEmpty() && !endDate.isEmpty()) {
        q.prepare("SELECT substr(start_time, 1, 10) as d, app_name, SUM(duration_seconds) as total_seconds "
                  "FROM sessions WHERE start_time >= ? AND start_time < ? "
                  "AND duration_seconds >= ? "
                  "AND NOT EXISTS (SELECT 1 FROM ignored_apps ia "
                  "WHERE sessions.process_key = ia.process_name) "
                  "GROUP BY d, app_name ORDER BY d ASC, total_seconds DESC");
        q.addBindValue(startDate);
        q.addBindValue(dayEndExclusive(endDate));
        q.addBindValue(threshold);
    } else {
        q.prepare("SELECT substr(start_time, 1, 10) as d, app_name, SUM(duration_seconds) as total_seconds "
                  "FROM sessions WHERE NOT EXISTS (SELECT 1 FROM ignored_apps ia "
                  "WHERE sessions.process_key = ia.process_name) "
                  "AND duration_seconds >= ? "
                  "GROUP BY d, app_name ORDER BY d ASC, total_seconds DESC");
        q.addBindValue(threshold);
    }
    q.exec();
    QVector<QVariantMap> results;
    while (q.next()) {
        QVariantMap row;
        row["d"] = q.value("d");
        row["app_name"] = q.value("app_name");
        row["total_seconds"] = q.value("total_seconds");
        results.append(row);
    }
    mergeAppNameVariants(results);
    std::sort(results.begin(), results.end(),
              [](const QVariantMap &a, const QVariantMap &b) {
                  const int cmp = a.value(QStringLiteral("d")).toString().compare(
                      b.value(QStringLiteral("d")).toString());
                  if (cmp != 0)
                      return cmp < 0;
                  return a.value(QStringLiteral("total_seconds")).toInt() >
                         b.value(QStringLiteral("total_seconds")).toInt();
              });
    return results;
}

QString DatabaseManager::getSetting(const QString &key, const QString &defaultValue)
{
    QMutexLocker lock(&m_mutex);
    return getSettingLocked(key, defaultValue);
}

QString DatabaseManager::getSettingLocked(const QString &key, const QString &defaultValue)
{
    QSqlQuery q(m_db);
    q.prepare("SELECT value FROM settings WHERE key = ?");
    q.addBindValue(key);
    q.exec();
    if (q.next())
        return q.value("value").toString();
    return defaultValue;
}

void DatabaseManager::setSetting(const QString &key, const QString &value)
{
    QMutexLocker lock(&m_mutex);
    QSqlQuery q(m_db);
    q.prepare("INSERT OR REPLACE INTO settings (key, value) VALUES (?, ?)");
    q.addBindValue(key);
    q.addBindValue(value);
    if (!q.exec())
        qWarning() << "setSetting failed:" << q.lastError();
}

QMap<int, QString> DatabaseManager::getIgnoredApps()
{
    QMutexLocker lock(&m_mutex);
    QMap<int, QString> result;
    QSqlQuery q(m_db);
    q.exec("SELECT id, process_name FROM ignored_apps ORDER BY process_name");
    while (q.next())
        result.insert(q.value("id").toInt(), q.value("process_name").toString());
    return result;
}

int DatabaseManager::addIgnoredApp(const QString &processName)
{
    QMutexLocker lock(&m_mutex);
    const QString processKey = ProcessIdentity::normalizeKey(processName);
    if (processKey.isEmpty())
        return -1;
    QSqlQuery q(m_db);
    q.prepare("SELECT id FROM ignored_apps WHERE process_name = ?");
    q.addBindValue(processKey);
    q.exec();
    if (q.next())
        return q.value("id").toInt();
    q.prepare("INSERT INTO ignored_apps (process_name) VALUES (?)");
    q.addBindValue(processKey);
    if (!q.exec()) {
        qWarning() << "addIgnoredApp failed:" << q.lastError();
        return -1;
    }
    return q.lastInsertId().toInt();
}

void DatabaseManager::removeIgnoredApp(int id)
{
    QMutexLocker lock(&m_mutex);
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM ignored_apps WHERE id = ?");
    q.addBindValue(id);
    if (!q.exec())
        qWarning() << "removeIgnoredApp failed:" << q.lastError();
}

QMap<QString, QString> DatabaseManager::getAppAliases()
{
    QMutexLocker lock(&m_mutex);
    QMap<QString, QString> result;
    QSqlQuery q(m_db);
    q.exec("SELECT process_name, display_name FROM app_aliases");
    while (q.next())
        result.insert(q.value("process_name").toString(), q.value("display_name").toString());
    return result;
}

int DatabaseManager::setAppAlias(const QString &processName, const QString &displayName)
{
    QMutexLocker lock(&m_mutex);
    const QString processKey = ProcessIdentity::normalizeKey(processName);
    if (processKey.isEmpty())
        return -1;
    QSqlQuery q(m_db);
    q.prepare("INSERT OR REPLACE INTO app_aliases (process_name, display_name) VALUES (?, ?)");
    q.addBindValue(processKey);
    q.addBindValue(displayName);
    if (!q.exec()) {
        qWarning() << "setAppAlias failed:" << q.lastError();
        return -1;
    }
    return q.lastInsertId().toInt();
}

void DatabaseManager::removeAppAlias(int id)
{
    QMutexLocker lock(&m_mutex);
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM app_aliases WHERE id = ?");
    q.addBindValue(id);
    if (!q.exec())
        qWarning() << "removeAppAlias failed:" << q.lastError();
}

void DatabaseManager::removeAppAliasByProcessName(const QString &processName)
{
    QMutexLocker lock(&m_mutex);
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM app_aliases WHERE process_name = ?");
    q.addBindValue(ProcessIdentity::normalizeKey(processName));
    if (!q.exec())
        qWarning() << "removeAppAliasByProcessName failed:" << q.lastError();
}

QStringList DatabaseManager::getAllKnownProcessNames()
{
    QMutexLocker lock(&m_mutex);
    QStringList result;
    QSqlQuery q(m_db);
    q.exec("SELECT DISTINCT process_name FROM sessions ORDER BY process_name");
    while (q.next())
        result.append(q.value("process_name").toString());
    return result;
}

bool DatabaseManager::isAppIgnored(const QString &processKey)
{
    QMutexLocker lock(&m_mutex);
    const QString key = ProcessIdentity::normalizeKey(processKey);
    if (key.isEmpty())
        return false;
    QSqlQuery q(m_db);
    q.prepare("SELECT 1 FROM ignored_apps WHERE process_name = ?");
    q.addBindValue(key);
    q.exec();
    return q.next();
}

QVector<QVariantMap> DatabaseManager::getGroups()
{
    QMutexLocker lock(&m_mutex);
    QVector<QVariantMap> result;
    QSqlQuery q(m_db);
    // 成员数用相关子查询统计，避免 JOIN 把空组别从结果里过滤掉。
    q.exec("SELECT g.id, g.name, g.builtin, "
           "(SELECT COUNT(*) FROM app_group_members m WHERE m.group_id = g.id) AS members "
           "FROM app_groups g ORDER BY g.sort_order ASC, g.id ASC");
    while (q.next()) {
        QVariantMap row;
        row[QStringLiteral("id")] = q.value("id");
        row[QStringLiteral("name")] = q.value("name");
        row[QStringLiteral("builtin")] = q.value("builtin").toInt() != 0;
        row[QStringLiteral("members")] = q.value("members");
        result.append(row);
    }
    return result;
}

int DatabaseManager::addGroup(const QString &name)
{
    QMutexLocker lock(&m_mutex);
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty())
        return -1;
    QSqlQuery q(m_db);
    q.prepare("SELECT id FROM app_groups WHERE name = ?");
    q.addBindValue(trimmed);
    q.exec();
    if (q.next())
        return q.value("id").toInt();
    q.prepare("SELECT COALESCE(MAX(sort_order), 0) + 1 FROM app_groups");
    q.exec();
    int nextOrder = 1;
    if (q.next())
        nextOrder = q.value(0).toInt();
    q.prepare("INSERT INTO app_groups (name, sort_order, builtin) VALUES (?, ?, 0)");
    q.addBindValue(trimmed);
    q.addBindValue(nextOrder);
    if (!q.exec()) {
        qWarning() << "addGroup failed:" << q.lastError();
        return -1;
    }
    return q.lastInsertId().toInt();
}

void DatabaseManager::renameGroup(int id, const QString &name)
{
    QMutexLocker lock(&m_mutex);
    const QString trimmed = name.trimmed();
    if (id <= 0 || trimmed.isEmpty())
        return;
    QSqlQuery q(m_db);
    q.prepare("UPDATE app_groups SET name = ? WHERE id = ?");
    q.addBindValue(trimmed);
    q.addBindValue(id);
    if (!q.exec())
        qWarning() << "renameGroup failed:" << q.lastError();
}

void DatabaseManager::removeGroup(int id)
{
    QMutexLocker lock(&m_mutex);
    if (id <= 0)
        return;
    QSqlQuery q(m_db);
    // 成员不清空会留下指向已删除组别的孤儿行，表现为应用凭空消失。
    q.prepare("DELETE FROM app_group_members WHERE group_id = ?");
    q.addBindValue(id);
    if (!q.exec())
        qWarning() << "removeGroup members failed:" << q.lastError();
    q.prepare("DELETE FROM app_groups WHERE id = ?");
    q.addBindValue(id);
    if (!q.exec())
        qWarning() << "removeGroup failed:" << q.lastError();
}

void DatabaseManager::setAppGroup(const QString &processKey, int groupId)
{
    QMutexLocker lock(&m_mutex);
    const QString key = ProcessIdentity::normalizeKey(processKey);
    if (key.isEmpty())
        return;
    QSqlQuery q(m_db);
    if (groupId < 0) {
        q.prepare("DELETE FROM app_group_members WHERE process_key = ?");
        q.addBindValue(key);
        if (!q.exec())
            qWarning() << "setAppGroup clear failed:" << q.lastError();
        return;
    }
    q.prepare("INSERT OR REPLACE INTO app_group_members (process_key, group_id) VALUES (?, ?)");
    q.addBindValue(key);
    q.addBindValue(groupId);
    if (!q.exec())
        qWarning() << "setAppGroup failed:" << q.lastError();
}

QMap<QString, int> DatabaseManager::getAppGroupMembers()
{
    QMutexLocker lock(&m_mutex);
    QMap<QString, int> result;
    QSqlQuery q(m_db);
    q.exec("SELECT process_key, group_id FROM app_group_members");
    while (q.next())
        result.insert(q.value("process_key").toString(), q.value("group_id").toInt());
    return result;
}

QMap<QString, QString> DatabaseManager::getAppMerges()
{
    QMutexLocker lock(&m_mutex);
    QMap<QString, QString> result;
    QSqlQuery q(m_db);
    q.exec("SELECT process_key, target_key FROM app_merges");
    while (q.next())
        result.insert(q.value("process_key").toString(), q.value("target_key").toString());
    return result;
}

bool DatabaseManager::setAppMerge(const QString &sourceKey, const QString &targetKey)
{
    QMutexLocker lock(&m_mutex);
    const QString source = ProcessIdentity::normalizeKey(sourceKey);
    const QString target = ProcessIdentity::normalizeKey(targetKey);
    if (source.isEmpty() || target.isEmpty() || source == target)
        return false;

    QHash<QString, QString> merges;
    QSqlQuery q(m_db);
    q.exec("SELECT process_key, target_key FROM app_merges");
    while (q.next())
        merges.insert(q.value(0).toString(), q.value(1).toString());

    // 目标本身已是被合并的源时，改用目标的根，避免出现 A→B→C 多层链。
    const QString root = resolveMergeRoot(target, merges);
    if (root == source)
        return false;
    // 源是别的应用的合并目标时，把对方改指向新根，保持链深恒为 1。
    for (auto it = merges.begin(); it != merges.end(); ++it) {
        if (it.value() != source)
            continue;
        QSqlQuery rewire(m_db);
        rewire.prepare("UPDATE app_merges SET target_key = ? WHERE process_key = ?");
        rewire.addBindValue(root);
        rewire.addBindValue(it.key());
        if (!rewire.exec())
            qWarning() << "setAppMerge rewire failed:" << rewire.lastError();
    }

    q.prepare("INSERT OR REPLACE INTO app_merges (process_key, target_key) VALUES (?, ?)");
    q.addBindValue(source);
    q.addBindValue(root);
    if (!q.exec()) {
        qWarning() << "setAppMerge failed:" << q.lastError();
        return false;
    }
    return true;
}

void DatabaseManager::removeAppMerge(const QString &sourceKey)
{
    QMutexLocker lock(&m_mutex);
    const QString source = ProcessIdentity::normalizeKey(sourceKey);
    if (source.isEmpty())
        return;
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM app_merges WHERE process_key = ?");
    q.addBindValue(source);
    if (!q.exec())
        qWarning() << "removeAppMerge failed:" << q.lastError();
}

QString DatabaseManager::resolveMergeRoot(const QString &processKey,
                                          const QHash<QString, QString> &merges) const
{
    QString current = processKey;
    QSet<QString> seen;
    // 上限同时防环与防超长链：正常链深恒为 1，这里放宽到 32 仅作兜底。
    for (int depth = 0; depth < 32; ++depth) {
        const auto it = merges.constFind(current);
        if (it == merges.cend())
            return current;
        if (!seen.contains(current))
            seen.insert(current);
        else
            return current;
        current = it.value();
    }
    return current;
}

QVector<ResolvedApp> DatabaseManager::resolveAppTotals(const QString &startIso,
                                                       const QString &endIso,
                                                       int threshold, bool applyIgnored)
{
    // 调用方持有 m_mutex；这里只做查询与 C++ 侧身份解析。
    QSqlQuery q(m_db);
    QString sql =
        "SELECT process_key, process_name, app_name, "
        "SUM(duration_seconds) AS total_seconds "
        "FROM sessions WHERE duration_seconds >= ? ";
    if (!startIso.isEmpty() && !endIso.isEmpty())
        sql += "AND start_time >= ? AND start_time < ? ";
    if (applyIgnored) {
        sql += "AND NOT EXISTS (SELECT 1 FROM ignored_apps ia "
               "WHERE sessions.process_key = ia.process_name) ";
    }
    sql += "GROUP BY process_key, app_name";
    q.prepare(sql);
    q.addBindValue(threshold);
    if (!startIso.isEmpty() && !endIso.isEmpty()) {
        q.addBindValue(startIso);
        q.addBindValue(dayEndExclusive(endIso));
    }
    if (!q.exec()) {
        qWarning() << "resolveAppTotals failed:" << q.lastError();
        return {};
    }

    // 同一 process_key 下可能有多个 app_name 变体（大小 FileDescription/兜底名差异），
    // 先按 键+变体 收下来，再在下面挑组内最大变体作显示名。
    struct Variant {
        QString appName;
        QString processName;
        int seconds = 0;
    };
    QHash<QString, QVector<Variant>> byKey;
    while (q.next()) {
        QString key = q.value("process_key").toString();
        if (key.isEmpty())
            key = ProcessIdentity::normalizeKey(q.value("process_name").toString());
        if (key.isEmpty())
            continue;
        Variant v;
        v.appName = q.value("app_name").toString();
        v.processName = q.value("process_name").toString();
        v.seconds = q.value("total_seconds").toInt();
        byKey[key].append(v);
    }

    QHash<QString, QString> merges;
    QSqlQuery mergeQuery(m_db);
    mergeQuery.exec("SELECT process_key, target_key FROM app_merges");
    while (mergeQuery.next())
        merges.insert(mergeQuery.value(0).toString(), mergeQuery.value(1).toString());

    QMap<QString, QString> aliases;
    QSqlQuery aliasQuery(m_db);
    aliasQuery.exec("SELECT process_name, display_name FROM app_aliases");
    while (aliasQuery.next())
        aliases.insert(aliasQuery.value(0).toString(), aliasQuery.value(1).toString());

    QMap<QString, int> members;
    QSqlQuery memberQuery(m_db);
    memberQuery.exec("SELECT process_key, group_id FROM app_group_members");
    while (memberQuery.next())
        members.insert(memberQuery.value(0).toString(), memberQuery.value(1).toInt());

    // 先把每个源键的时长按根键归并，再选显示名：显示名必须来自全组，
    // 否则被合并掉的那个应用如果自己时长更大，名字会盖掉目标的别名。
    struct Accum {
        int seconds = 0;
        int groupId = -1;
        QString bestName;
        QString bestProcess;
        int bestSeconds = -1;
        int bestGroupSeconds = -1;
    };
    QHash<QString, Accum> byRoot;
    for (auto it = byKey.cbegin(); it != byKey.cend(); ++it) {
        const QString root = resolveMergeRoot(it.key(), merges);
        Accum &acc = byRoot[root];
        int keySeconds = 0;
        for (const Variant &v : it.value()) {
            keySeconds += v.seconds;
            if (v.seconds > acc.bestSeconds) {
                acc.bestSeconds = v.seconds;
                acc.bestName = v.appName;
                acc.bestProcess = v.processName;
            }
        }
        acc.seconds += keySeconds;
        const auto groupIt = members.constFind(it.key());
        if (groupIt != members.cend() && keySeconds > acc.bestGroupSeconds) {
            acc.bestGroupSeconds = keySeconds;
            acc.groupId = groupIt.value();
        }
    }

    QVector<ResolvedApp> result;
    result.reserve(byRoot.size());
    for (auto it = byRoot.begin(); it != byRoot.end(); ++it) {
        if (it.value().seconds <= 0)
            continue;
        ResolvedApp app;
        app.key = it.key();
        app.seconds = it.value().seconds;
        app.groupId = it.value().groupId;
        app.processName = it.value().bestProcess;
        const auto aliasIt = aliases.constFind(it.key());
        app.displayName = aliasIt != aliases.cend() && !aliasIt.value().trimmed().isEmpty()
                              ? aliasIt.value()
                              : it.value().bestName;
        if (app.displayName.isEmpty())
            app.displayName = app.key;
        result.append(app);
    }
    std::sort(result.begin(), result.end(),
              [](const ResolvedApp &a, const ResolvedApp &b) {
                  if (a.seconds != b.seconds)
                      return a.seconds > b.seconds;
                  return a.displayName.localeAwareCompare(b.displayName) < 0;
              });
    return result;
}

QVector<AppEntry> DatabaseManager::getManagedApps()
{
    QMutexLocker lock(&m_mutex);
    QSqlQuery q(m_db);
    // 与统计查询刻意不同：不做阈值与屏蔽过滤，管理界面要能看到被屏蔽
    // 和只有极短会话的应用本身，否则用户无从解除屏蔽。
    q.exec("SELECT process_key, process_name, app_name, "
           "SUM(duration_seconds) AS total_seconds, COUNT(*) AS sessions "
           "FROM sessions GROUP BY process_key, app_name ORDER BY process_key ASC");

    struct Variant {
        QString appName;
        QString processName;
        int seconds = 0;
        int sessions = 0;
    };
    QHash<QString, QVector<Variant>> byKey;
    while (q.next()) {
        QString key = q.value("process_key").toString();
        if (key.isEmpty())
            key = ProcessIdentity::normalizeKey(q.value("process_name").toString());
        if (key.isEmpty())
            continue;
        Variant v;
        v.appName = q.value("app_name").toString();
        v.processName = q.value("process_name").toString();
        v.seconds = q.value("total_seconds").toInt();
        v.sessions = q.value("sessions").toInt();
        byKey[key].append(v);
    }

    QHash<QString, QString> merges;
    QSqlQuery mergeQuery(m_db);
    mergeQuery.exec("SELECT process_key, target_key FROM app_merges");
    while (mergeQuery.next())
        merges.insert(mergeQuery.value(0).toString(), mergeQuery.value(1).toString());

    QMap<QString, QString> aliases;
    QSqlQuery aliasQuery(m_db);
    aliasQuery.exec("SELECT process_name, display_name FROM app_aliases");
    while (aliasQuery.next())
        aliases.insert(aliasQuery.value(0).toString(), aliasQuery.value(1).toString());

    QMap<QString, int> members;
    QSqlQuery memberQuery(m_db);
    memberQuery.exec("SELECT process_key, group_id FROM app_group_members");
    while (memberQuery.next())
        members.insert(memberQuery.value(0).toString(), memberQuery.value(1).toInt());

    QSet<QString> ignored;
    QSqlQuery ignoredQuery(m_db);
    ignoredQuery.exec("SELECT process_name FROM ignored_apps");
    while (ignoredQuery.next())
        ignored.insert(ignoredQuery.value(0).toString());

    // 屏蔽但从未产生会话的应用也要出现在列表里，否则无法解除屏蔽。
    for (const QString &key : ignored)
        if (!byKey.contains(key))
            byKey[key].append(Variant{});

    QVector<AppEntry> result;
    result.reserve(byKey.size());
    for (auto it = byKey.cbegin(); it != byKey.cend(); ++it) {
        AppEntry entry;
        entry.processKey = it.key();
        entry.ignored = ignored.contains(it.key());
        const auto mergeIt = merges.constFind(it.key());
        if (mergeIt != merges.cend())
            entry.mergedInto = mergeIt.value();
        const auto groupIt = members.constFind(it.key());
        if (groupIt != members.cend())
            entry.groupId = groupIt.value();

        int bestSeconds = -1;
        for (const Variant &v : it.value()) {
            entry.totalSeconds += v.seconds;
            entry.sessionCount += v.sessions;
            if (v.seconds > bestSeconds) {
                bestSeconds = v.seconds;
                entry.processName = v.processName;
                entry.displayName = v.appName;
            }
        }

        // 作为合并目标时把并入方的应用时长一并计入，与管理界面「合并后
        // 的总时长」语义一致；被并入的一侧仍统计自身的原始时长。
        for (auto src = merges.cbegin(); src != merges.cend(); ++src) {
            if (src.value() != it.key())
                continue;
            const auto target = byKey.constFind(src.key());
            if (target == byKey.cend())
                continue;
            for (const Variant &v : target.value()) {
                entry.totalSeconds += v.seconds;
                entry.sessionCount += v.sessions;
            }
        }
        const auto aliasIt = aliases.constFind(it.key());
        if (aliasIt != aliases.cend() && !aliasIt.value().trimmed().isEmpty())
            entry.displayName = aliasIt.value();
        if (entry.displayName.isEmpty()) {
            entry.displayName = entry.processName.isEmpty()
                                    ? entry.processKey
                                    : friendlyProcessName(entry.processName);
        }
        result.append(entry);
    }
    std::sort(result.begin(), result.end(),
              [](const AppEntry &a, const AppEntry &b) {
                  return a.displayName.localeAwareCompare(b.displayName) < 0;
              });
    return result;
}

QVector<QVariantMap> DatabaseManager::groupRankLocked(const QString &startIso,
                                                      const QString &endIso)
{
    const int threshold = getSettingLocked("min_record_threshold", "40").toInt();
    const QVector<ResolvedApp> apps =
        resolveAppTotals(startIso, endIso, threshold, true);

    QMap<int, QString> groupNames;
    QSqlQuery groupQuery(m_db);
    groupQuery.exec("SELECT id, name FROM app_groups");
    while (groupQuery.next())
        groupNames.insert(groupQuery.value(0).toInt(), groupQuery.value(1).toString());

    // 用户尚未把任何应用归入组别时，整份数据只会坍缩成一个「未分组」桶，
    // 与应用排行重复；此时返回空，由调用方（卡片/报告）隐藏组别维度。
    QSqlQuery memberQuery(m_db);
    memberQuery.exec("SELECT COUNT(*) FROM app_group_members");
    int memberCount = 0;
    if (memberQuery.next())
        memberCount = memberQuery.value(0).toInt();
    if (memberCount == 0)
        return {};

    // 未归入任何组别的应用汇入「未分组」，保证组别排行之和等于今日总时长。
    const QString ungroupedLabel = QString::fromUtf8(
        "\xe6\x9c\xaa\xe5\x88\x86\xe7\xbb\x84");
    QMap<QString, int> totals;   // 组别显示名 -> 秒数
    QMap<QString, QString> labelFor;
    for (const ResolvedApp &app : apps) {
        const auto it = groupNames.constFind(app.groupId);
        const QString label = it != groupNames.cend() ? it.value() : ungroupedLabel;
        totals[label] += app.seconds;
        labelFor.insert(label, label);
    }

    QVector<QVariantMap> result;
    for (auto it = totals.cbegin(); it != totals.cend(); ++it) {
        QVariantMap row;
        row[QStringLiteral("app_name")] = it.key();
        row[QStringLiteral("group_name")] = it.key();
        row[QStringLiteral("total_seconds")] = it.value();
        row[QStringLiteral("is_group")] = true;
        result.append(row);
    }
    std::sort(result.begin(), result.end(),
              [](const QVariantMap &a, const QVariantMap &b) {
                  return a.value(QStringLiteral("total_seconds")).toInt() >
                         b.value(QStringLiteral("total_seconds")).toInt();
              });
    return result;
}

QVector<QVariantMap> DatabaseManager::getGroupRank(const QDate &targetDate)
{
    QMutexLocker lock(&m_mutex);
    return groupRankLocked(targetDate.toString(Qt::ISODate),
                           targetDate.toString(Qt::ISODate));
}

QVector<QVariantMap> DatabaseManager::getGroupRank(const QString &startDate,
                                                   const QString &endDate)
{
    QMutexLocker lock(&m_mutex);
    return groupRankLocked(startDate, endDate);
}

void DatabaseManager::close()
{
    QMutexLocker lock(&m_mutex);
    if (m_closed) return;
    m_closed = true;
    QString connName = m_db.connectionName();
    if (m_db.isOpen())
        m_db.close();
    m_db = QSqlDatabase();
    if (!connName.isEmpty())
        QSqlDatabase::removeDatabase(connName);
}
