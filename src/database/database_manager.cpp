#include "database_manager.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDir>
#include <QStandardPaths>
#include <QUuid>

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
    migrate();
}

DatabaseManager::~DatabaseManager()
{
    close();
}

void DatabaseManager::migrate()
{
    QSqlQuery q(m_db);
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
    q.exec("CREATE INDEX IF NOT EXISTS idx_sessions_start ON sessions(start_time)");
    q.exec("CREATE INDEX IF NOT EXISTS idx_sessions_app ON sessions(app_name)");

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

    q.exec("INSERT OR IGNORE INTO settings (key, value) VALUES ('tracking_enabled', 'true')");
    q.exec("INSERT OR IGNORE INTO settings (key, value) VALUES ('poll_interval', '1')");
    q.exec("INSERT OR IGNORE INTO settings (key, value) VALUES ('idle_threshold', '60')");
    q.exec("INSERT OR IGNORE INTO settings (key, value) VALUES ('auto_start', 'false')");
}

qint64 DatabaseManager::insertSession(const QString &processName, const QString &windowTitle,
                                       const QString &appName, const QDateTime &startTime,
                                       const QDateTime &endTime, int durationSeconds)
{
    QMutexLocker lock(&m_mutex);
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO sessions (process_name, window_title, app_name, start_time, end_time, duration_seconds) "
              "VALUES (?, ?, ?, ?, ?, ?)");
    q.addBindValue(processName);
    q.addBindValue(windowTitle);
    q.addBindValue(appName);
    q.addBindValue(startTime.toString(Qt::ISODate));
    q.addBindValue(endTime.isValid() ? endTime.toString(Qt::ISODate) : QVariant());
    q.addBindValue(durationSeconds);
    if (!q.exec())
        qWarning() << "insertSession failed:" << q.lastError();
    return q.lastInsertId().toLongLong();
}

void DatabaseManager::updateSessionEnd(qint64 sessionId, const QDateTime &endTime, int durationSeconds)
{
    QMutexLocker lock(&m_mutex);
    QSqlQuery q(m_db);
    q.prepare("UPDATE sessions SET end_time=?, duration_seconds=? WHERE id=?");
    q.addBindValue(endTime.toString(Qt::ISODate));
    q.addBindValue(durationSeconds);
    q.addBindValue(sessionId);
    if (!q.exec())
        qWarning() << "updateSessionEnd failed:" << q.lastError();
}

void DatabaseManager::updateSessionDuration(qint64 sessionId, int durationSeconds)
{
    QMutexLocker lock(&m_mutex);
    QSqlQuery q(m_db);
    q.prepare("UPDATE sessions SET duration_seconds=? WHERE id=?");
    q.addBindValue(durationSeconds);
    q.addBindValue(sessionId);
    if (!q.exec())
        qWarning() << "updateSessionDuration failed:" << q.lastError();
}

QVector<QVariantMap> DatabaseManager::getTodaySummary()
{
    int threshold = getSetting("min_record_threshold", "40").toInt();
    QMutexLocker lock(&m_mutex);

    QSqlQuery q(m_db);
    q.prepare("SELECT app_name, SUM(duration_seconds) as total_seconds "
              "FROM sessions WHERE date(start_time) = ? "
              "AND duration_seconds >= ? "
              "AND NOT EXISTS (SELECT 1 FROM ignored_apps ia "
              "WHERE sessions.process_name = ia.process_name "
              "OR sessions.process_name LIKE '%\\' || ia.process_name) "
              "GROUP BY app_name ORDER BY total_seconds DESC");
    q.addBindValue(QDate::currentDate().toString(Qt::ISODate));
    q.addBindValue(threshold);
    q.exec();
    QVector<QVariantMap> results;
    while (q.next()) {
        QVariantMap row;
        row["app_name"] = q.value("app_name");
        row["total_seconds"] = q.value("total_seconds");
        results.append(row);
    }
    return results;
}

int DatabaseManager::getTodayTotal()
{
    int threshold = getSetting("min_record_threshold", "40").toInt();
    QMutexLocker lock(&m_mutex);

    QSqlQuery q(m_db);
    q.prepare("SELECT COALESCE(SUM(duration_seconds), 0) as total "
              "FROM sessions WHERE date(start_time) = ? "
              "AND duration_seconds >= ? "
              "AND NOT EXISTS (SELECT 1 FROM ignored_apps ia "
              "WHERE sessions.process_name = ia.process_name "
              "OR sessions.process_name LIKE '%\\' || ia.process_name)");
    QString todayStr = QDate::currentDate().toString(Qt::ISODate);
    q.addBindValue(todayStr);
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
              "FROM sessions WHERE date(start_time) = ? "
              "AND duration_seconds >= ? "
              "AND NOT EXISTS (SELECT 1 FROM ignored_apps ia "
              "WHERE sessions.process_name = ia.process_name "
              "OR sessions.process_name LIKE '%\\' || ia.process_name)");
    q.addBindValue(QDate::currentDate().addDays(-1).toString(Qt::ISODate));
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
    q.prepare("SELECT date(start_time) as d, SUM(duration_seconds) as total_seconds "
              "FROM sessions WHERE date(start_time) >= ? AND date(start_time) <= ? "
              "AND duration_seconds >= ? "
              "AND NOT EXISTS (SELECT 1 FROM ignored_apps ia "
              "WHERE sessions.process_name = ia.process_name "
              "OR sessions.process_name LIKE '%\\' || ia.process_name) "
              "GROUP BY date(start_time) ORDER BY d ASC");
    q.addBindValue(monday.toString(Qt::ISODate));
    q.addBindValue(today.toString(Qt::ISODate));
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
        "SELECT app_name, "
        "  (SELECT s2.process_name FROM sessions s2 "
        "   WHERE s2.app_name = s1.app_name AND date(s2.start_time) = ? "
        "   AND s2.duration_seconds >= ? "
        "   AND NOT EXISTS (SELECT 1 FROM ignored_apps ia "
        "   WHERE s2.process_name = ia.process_name "
        "   OR s2.process_name LIKE '%\\' || ia.process_name) "
        "   GROUP BY s2.process_name "
        "   ORDER BY SUM(s2.duration_seconds) DESC LIMIT 1) as process_name, "
        "  SUM(s1.duration_seconds) as total_seconds "
        "FROM sessions s1 WHERE date(s1.start_time) = ? "
        "AND s1.duration_seconds >= ? "
        "AND NOT EXISTS (SELECT 1 FROM ignored_apps ia "
        "WHERE s1.process_name = ia.process_name "
        "OR s1.process_name LIKE '%\\' || ia.process_name) "
        "GROUP BY s1.app_name ORDER BY total_seconds DESC");
    q.addBindValue(targetDate.toString(Qt::ISODate));
    q.addBindValue(threshold);
    q.addBindValue(targetDate.toString(Qt::ISODate));
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
    return results;
}

QVector<QVariantMap> DatabaseManager::getAllSessions(const QString &startDate, const QString &endDate)
{
    int threshold = getSetting("min_record_threshold", "40").toInt();
    QMutexLocker lock(&m_mutex);

    QSqlQuery q(m_db);
    if (!startDate.isEmpty() && !endDate.isEmpty()) {
        q.prepare("SELECT * FROM sessions WHERE date(start_time) >= ? AND date(start_time) <= ? "
                  "AND duration_seconds >= ? "
                  "AND NOT EXISTS (SELECT 1 FROM ignored_apps ia "
                  "WHERE sessions.process_name = ia.process_name "
                  "OR sessions.process_name LIKE '%\\' || ia.process_name) "
                  "ORDER BY start_time ASC");
        q.addBindValue(startDate);
        q.addBindValue(endDate);
        q.addBindValue(threshold);
    } else {
        q.prepare("SELECT * FROM sessions "
                  "WHERE duration_seconds >= ? "
                  "AND NOT EXISTS (SELECT 1 FROM ignored_apps ia "
                  "WHERE sessions.process_name = ia.process_name "
                  "OR sessions.process_name LIKE '%\\' || ia.process_name) "
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
        q.prepare("SELECT date(start_time) as d, app_name, SUM(duration_seconds) as total_seconds "
                  "FROM sessions WHERE date(start_time) >= ? AND date(start_time) <= ? "
                  "AND duration_seconds >= ? "
                  "AND NOT EXISTS (SELECT 1 FROM ignored_apps ia "
                  "WHERE sessions.process_name = ia.process_name "
                  "OR sessions.process_name LIKE '%\\' || ia.process_name) "
                  "GROUP BY d, app_name ORDER BY d ASC, total_seconds DESC");
        q.addBindValue(startDate);
        q.addBindValue(endDate);
        q.addBindValue(threshold);
    } else {
        q.prepare("SELECT date(start_time) as d, app_name, SUM(duration_seconds) as total_seconds "
                  "FROM sessions WHERE NOT EXISTS (SELECT 1 FROM ignored_apps ia "
                  "WHERE sessions.process_name = ia.process_name "
                  "OR sessions.process_name LIKE '%\\' || ia.process_name) "
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
    return results;
}

QString DatabaseManager::getSetting(const QString &key, const QString &defaultValue)
{
    QMutexLocker lock(&m_mutex);
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
    QSqlQuery q(m_db);
    q.prepare("SELECT id FROM ignored_apps WHERE process_name = ?");
    q.addBindValue(processName);
    q.exec();
    if (q.next())
        return q.value("id").toInt();
    q.prepare("INSERT INTO ignored_apps (process_name) VALUES (?)");
    q.addBindValue(processName);
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
    QSqlQuery q(m_db);
    q.prepare("INSERT OR REPLACE INTO app_aliases (process_name, display_name) VALUES (?, ?)");
    q.addBindValue(processName);
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
    q.addBindValue(processName);
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
