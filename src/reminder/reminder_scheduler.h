#ifndef REMINDER_SCHEDULER_H
#define REMINDER_SCHEDULER_H

#include <QObject>
#include <QString>
#include <QTime>
#include <QTimer>
#include <QSet>

class DatabaseManager;
class AiClient;

/// 定时提醒调度器：常驻托盘时按配置的多个时间点各触发一次提醒。
/// 提醒内容优先由 AiClient 生成短文案，AI 未配置或失败时回退本地统计模板，
/// 保证到点提醒始终可用。去重键为「日期|HH:mm」，同分钟只触发一次，
/// 跨天后自然失效，因此无需清理。
class ReminderScheduler : public QObject
{
    Q_OBJECT
public:
    explicit ReminderScheduler(DatabaseManager *db, AiClient *ai,
                               QObject *parent = nullptr);

    void start();
    void stop();
    void reloadSettings();

    /// 30 秒轮询定时器是否已启动（start 后 true，stop 后 false）。
    bool isRunning() const;

    /// 检查 now 时刻是否命中任一配置时间点并触发提醒。
    /// now 为空时使用当前时间；公开以便测试注入时间（定时器间隔 30 秒，测试不便等待）。
    void checkNow(const QTime &now = QTime());

signals:
    /// 到点提醒（title 为通知标题，message 为提醒正文）。
    void reminderDue(const QString &title, const QString &message);

private:
    void fireReminder(const QString &timeStr);
    QString buildLocalMessage() const;
    QString reminderTitle() const;
    static QString stripMarkdown(const QString &text);

    DatabaseManager *m_db;
    AiClient *m_ai;
    QTimer *m_timer = nullptr;
    bool m_enabled = false;
    QSet<QString> m_times;     // "HH:mm"
    QSet<QString> m_firedKeys; // "yyyy-MM-dd|HH:mm"，防止同分钟重复触发
};

#endif // REMINDER_SCHEDULER_H
