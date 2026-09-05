#ifndef DATABASE_MANAGER_H
#define DATABASE_MANAGER_H

#include <QString>
#include <QSqlDatabase>
#include <QMutex>
#include <QDateTime>
#include <QDate>
#include <QVariantMap>
#include <QVector>
#include <QHash>
#include <QSet>
#include <QMap>
#include <QStringList>
#include "tracker/tracking_store.h"

/// 应用管理界面的一行：以归一化进程键为身份主键，附带显示名、组别、合并目标、
/// 屏蔽态与累计统计。displayName 已按 别名 > 组内最大变体 解析。
struct AppEntry
{
    QString processKey;   // 归一化进程键（身份主键）
    QString processName;  // 该键下最近一次的完整进程路径（用于取图标）
    QString displayName;  // 解析后的显示名
    QString mergedInto;   // 被合并到的目标键；为空表示自身是根
    int groupId = -1;     // 所属组别 id；-1 表示未分组
    bool ignored = false;
    int totalSeconds = 0;
    int sessionCount = 0;
};

/// 解析后的应用级聚合：身份键为合并链根键，秒数已把源应用的时长并入。
struct ResolvedApp
{
    QString key;          // 合并链根键
    QString processName;  // 根键下累计时长最大变体的完整路径（取图标用）
    QString displayName;  // 别名命中值，否则组内最大变体名
    int seconds = 0;
    int groupId = -1;     // 根键所属组别；未设置时取最大贡献源键的组别
};

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
    QVector<QVariantMap> getMonthSummary();
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
    /// 按进程键判断是否被屏蔽（应用管理页据此回显开关状态）。
    bool isAppIgnored(const QString &processKey);

    QMap<QString, QString> getAppAliases();
    int setAppAlias(const QString &processName, const QString &displayName);
    void removeAppAlias(int id);
    void removeAppAliasByProcessName(const QString &processName);

    QStringList getAllKnownProcessNames();

    // ---- 应用管理：组别 / 合并 / 应用清单 ----

    /// 组别行 id / 名称 / 图标 / 成员数 / builtin，按 sort_order、id 排序。
    /// icon 已做兜底解析（空值按名称推导），恒为非空 emoji。
    QVector<QVariantMap> getGroups();
    /// 新建组别；icon 为空时按名称自动分配回退图标。
    int addGroup(const QString &name, const QString &icon = QString());
    /// 设置组别图标（emoji 文本）；传空串清除，下次启动迁移会按名称回填。
    void setGroupIcon(int id, const QString &icon);
    void renameGroup(int id, const QString &name);
    /// 删除组别并清空其成员（成员自动回落「未分组」）。
    void removeGroup(int id);
    /// 设置应用所属组别；groupId < 0 表示移出组别。
    void setAppGroup(const QString &processKey, int groupId);
    QMap<QString, int> getAppGroupMembers();

    QMap<QString, QString> getAppMerges();
    /// 建立合并映射；目标无效（自身、环、已是源的键）时返回 false。
    bool setAppMerge(const QString &sourceKey, const QString &targetKey);
    void removeAppMerge(const QString &sourceKey);

    /// 已追踪过的应用清单（含从未产生会话的屏蔽项），供应用管理界面使用。
    /// 时长为全期累计，不受 min_record_threshold 与屏蔽过滤影响——管理界面
    /// 需要看到被屏蔽和极短的应用本身。按累计时长降序、同值按显示名升序。
    QVector<AppEntry> getManagedApps();

    /// 指定进程键最近的去重窗口标题（按最近出现时间降序，最多 limit 条），
    /// 供「AI 识别应用」推断应用名。
    QStringList getRecentWindowTitles(const QString &processKey, int limit = 20);

    /// 组别排行：未归入任何组别的应用自动汇入「未分组」，全期无数据。
    QVector<QVariantMap> getGroupRank(const QDate &targetDate = QDate::currentDate());
    QVector<QVariantMap> getGroupRank(const QString &startDate, const QString &endDate);

    void close();

private:
    void migrate();
    /// 调用方已持 m_mutex 时的设置读取版本（m_mutex 非递归，内部再取锁会自锁）。
    QString getSettingLocked(const QString &key, const QString &defaultValue);
    /// 沿 app_merges 链解析到根键；出现环时返回链上最后的键（防死循环）。
    QString resolveMergeRoot(const QString &processKey,
                             const QHash<QString, QString> &merges) const;
    /// 统计核心：按 process_key 粗聚后解析合并/别名/组别，得到应用级聚合。
    /// startIso/endIso 为空表示全期；调用方须先取阈值并持有 m_mutex。
    QVector<ResolvedApp> resolveAppTotals(const QString &startIso, const QString &endIso,
                                          int threshold, bool applyIgnored);
    QVector<QVariantMap> groupRankLocked(const QString &startIso, const QString &endIso);
    QSqlDatabase m_db;
    QMutex m_mutex;
    bool m_closed = false;
    QString m_dbPath;
};

#endif // DATABASE_MANAGER_H
