#ifndef UPDATE_CHECKER_H
#define UPDATE_CHECKER_H

#include <QObject>
#include <QString>
#include <QMetaType>
#include <QTimer>
#include <QNetworkAccessManager>

class DatabaseManager;

/// GitHub Release 检查结果：一次成功解析出的最新版本信息。
struct UpdateInfo {
    QString version;      ///< 纯版本号，如 "5.6.5"（不含 v 前缀）
    QString tagName;      ///< GitHub 标记名，如 "v5.6.5"
    QString notes;        ///< Release 正文（markdown 原文）
    QString downloadUrl;  ///< 安装包直链（browser_download_url），无资产时为空
    QString releaseUrl;   ///< GitHub Release 页面地址
    QString publishedAt;  ///< 发布时间（yyyy-MM-dd），解析失败时为空

    bool operator==(const UpdateInfo &other) const
    {
        return version == other.version;
    }
};

Q_DECLARE_METATYPE(UpdateInfo)

/// 定期检查 GitHub Releases 是否有新版本的更新服务。
///
/// - 启动后被 start() 安排：延迟 60 秒首次检查，此后每 24 小时一次；
/// - 检查成功把结果缓存在 settings 表（update_latest_*），失败静默；
/// - 自动检查发现新版本且该版本从未提醒过用户时发出 updateAvailable
///   （提醒版本记录于 update_notified_version，跨重启去重）；
/// - 手动检查 checkNow(true) 只发 checkFinished，弹窗由调用方负责，
///   避免与自动路径的 updateAvailable 弹窗重复。
class UpdateChecker : public QObject
{
    Q_OBJECT
public:
    explicit UpdateChecker(DatabaseManager *db, QObject *parent = nullptr);

    /// 安排自动检查：60 秒后首次，此后每 24 小时。
    void start();
    void stop();

    /// 立即检查一次。manual 为 true 表示用户手动触发（不产生 updateAvailable）。
    /// 已有请求在途时返回 false 且不发起新请求。
    bool checkNow(bool manual = false);

    /// 上次成功检查的缓存结果（未检查过时 version 为空）。
    UpdateInfo cachedInfo() const;

    /// 远程版本高于本地版本时返回 true；任一版本段不是纯数字时返回 false
    /// （无法判断则视为无更新，避免误报）。
    static bool isNewerVersion(const QString &remote, const QString &local);

    /// 解析 GitHub releases/latest 响应；失败时 *ok 置 false 并返回空信息。
    static UpdateInfo parseRelease(const QByteArray &json, bool *ok);

    /// 测试专用：替换 API 端点（默认 GitHub releases/latest）。
    void setApiUrlForTest(const QString &url);

signals:
    /// 每次检查完成（手动与自动都会发出）。
    void checkFinished(bool hasUpdate, const UpdateInfo &info,
                       const QString &error);
    /// 自动检查发现新版本且该版本尚未提醒过用户。
    void updateAvailable(const UpdateInfo &info);

private slots:
    void onReplyFinished();

private:
    void finishCheck(const UpdateInfo &info, bool hasUpdate,
                     const QString &error);
    void persistResult(const UpdateInfo &info);

    DatabaseManager *m_db;
    QNetworkAccessManager *m_nam;
    QTimer *m_timer;
    QNetworkReply *m_reply = nullptr;
    bool m_manual = false;
    QString m_apiUrl;
};

#endif // UPDATE_CHECKER_H
