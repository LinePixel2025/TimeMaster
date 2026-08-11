#ifndef DATABASE_MANAGER_H
#define DATABASE_MANAGER_H

#include <QString>
#include <QSqlDatabase>
#include <QMutex>
#include <QDateTime>
#include <QDate>
#include <QVariantMap>
#include <QVector>
#include <QSet>
#include <QMap>
#include <QStringList>
#include "tracker/tracking_store.h"

class DatabaseManager : public TrackingStore
{
public:
    explicit DatabaseManager(const QString &dbPath = QString());
    ~DatabaseManager() override;

    qint64 insertSession(const QString &processName, const QString &windowTitle,
                         const QString &appName, const QDateTime &startTime,
                         const QDateTime &endTime, int durationSeconds) override;
    bool updateSessionEnd(qint64 sessionId, const QDateTime &endTime,
                          int durationSeconds) override;
    bool updateSessionDuration(qint64 sessionId, int durationSeconds) override;

    QString databasePath() const;

    QVector<QVariantMap> getTodaySummary();
    int getTodayTotal();
    int getYesterdayTotal();
    QVector<QVariantMap> getWeekSummary();
    QVector<QVariantMap> getAppRank(const QDate &targetDate = QDate::currentDate());
    QVector<QVariantMap> getAllSessions(const QString &startDate = QString(),
                                        const QString &endDate = QString());
    QVector<QVariantMap> getDailySummaries(const QString &startDate = QString(),
                                            const QString &endDate = QString());

    QString getSetting(const QString &key, const QString &defaultValue = QString());
    void setSetting(const QString &key, const QString &value);

    QMap<int, QString> getIgnoredApps();
    int addIgnoredApp(const QString &processName);
    void removeIgnoredApp(int id);

    QMap<QString, QString> getAppAliases();
    int setAppAlias(const QString &processName, const QString &displayName);
    void removeAppAlias(int id);
    void removeAppAliasByProcessName(const QString &processName);

    QStringList getAllKnownProcessNames();

    void close();

private:
    void migrate();
    QSqlDatabase m_db;
    QMutex m_mutex;
    bool m_closed = false;
    QString m_dbPath;
};

#endif // DATABASE_MANAGER_H
