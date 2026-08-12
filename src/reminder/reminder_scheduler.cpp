#include "reminder/reminder_scheduler.h"
#include "ai/ai_client.h"
#include "database/database_manager.h"

#include <QDate>
#include <QDateTime>
#include <QDebug>
#include <QRegularExpression>
#include <QStringList>
#include <QTimer>

ReminderScheduler::ReminderScheduler(DatabaseManager *db, AiClient *ai,
                                     QObject *parent)
    : QObject(parent), m_db(db), m_ai(ai)
{
    m_timer = new QTimer(this);
    m_timer->setInterval(30000);
    connect(m_timer, &QTimer::timeout, this, [this]() { checkNow(); });

    // 将 AI 提醒结果接回提醒流程：tag 不匹配（已被新提醒取代）时丢弃。
    connect(m_ai, &AiClient::reminderReady, this,
            [this](const QString &tag, const QString &text) {
        if (m_firedKeys.contains(tag))
            emit reminderDue(reminderTitle(), stripMarkdown(text));
    });
    connect(m_ai, &AiClient::reminderFailed, this,
            [this](const QString &tag, const QString &) {
        // AI 失败 → 回退本地统计模板，保证提醒不落空。
        if (m_firedKeys.contains(tag))
            emit reminderDue(reminderTitle(), buildLocalMessage());
    });
}

void ReminderScheduler::start()
{
    reloadSettings();
    m_timer->start(); // 启动 30 秒轮询定时器；漏掉将导致只在启动瞬间检查一次。
    checkNow();
}

void ReminderScheduler::stop()
{
    m_timer->stop();
}

bool ReminderScheduler::isRunning() const
{
    return m_timer->isActive();
}

void ReminderScheduler::reloadSettings()
{
    m_enabled = (m_db->getSetting("reminder_enabled", "false") == "true");
    m_times.clear();
    const QStringList parts =
        m_db->getSetting("reminder_times", "").split(QLatin1Char(','));
    for (const QString &part : parts) {
        const QString time = part.trimmed();
        if (!time.isEmpty() &&
            QTime::fromString(time, QStringLiteral("HH:mm")).isValid())
            m_times.insert(time);
    }
}

void ReminderScheduler::checkNow(const QTime &now)
{
    if (!m_enabled || m_times.isEmpty())
        return;

    const QTime current = now.isValid() ? now : QTime::currentTime();
    const QString timeStr = current.toString(QStringLiteral("HH:mm"));
    if (!m_times.contains(timeStr))
        return;
    fireReminder(timeStr);
}

void ReminderScheduler::fireReminder(const QString &timeStr)
{
    // 去重键含日期：跨天后同分钟可再次提醒；同分钟内定时器不会重复触发。
    const QString key = QDate::currentDate().toString(Qt::ISODate)
                        + QLatin1Char('|') + timeStr;
    if (m_firedKeys.contains(key))
        return;
    m_firedKeys.insert(key);

    // 今日完全无记录时不发真实提醒，但弹一条说明，避免"配置了却不响"的困惑；
    // 同时记录触发痕迹供设置界面排查。
    if (m_db->getTodayTotal() <= 0) {
        m_db->setSetting("reminder_last_fired",
                         QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")
                         + QStringLiteral(" 到点，今日暂无使用记录，未提醒"));
        emit reminderDue(reminderTitle(),
                         QStringLiteral("今日暂无使用记录，本次到点提醒未触发。"));
        return;
    }

    // AI 已配置且有今日数据 → 异步生成；未配置则由 AI 客户端直接短路返回 false。
    if (m_ai->isConfigured() && m_ai->generateReminderMessage(key)) {
        m_db->setSetting("reminder_last_fired",
                         QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")
                         + QStringLiteral(" 到点，AI 文案生成中"));
        return; // 结果经 reminderReady/reminderFailed 回填。
    }
    m_db->setSetting("reminder_last_fired",
                     QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")
                     + QStringLiteral(" 到点，已提醒"));
    emit reminderDue(reminderTitle(), buildLocalMessage());
}

QString ReminderScheduler::buildLocalMessage() const
{
    const int total = m_db->getTodayTotal();
    const int goal = m_db->getSetting("daily_goal", "28800").toInt();

    const auto rank = m_db->getAppRank();
    QString topApp;
    if (!rank.isEmpty())
        topApp = rank.first()[QStringLiteral("app_name")].toString();

    const QString minutes = QString::number(total / 60);
    QString message;
    if (total >= goal && goal > 0) {
        const int over = (total - goal) / 60;
        message = QStringLiteral("今日已使用 %1 分钟，已超出目标 %2 分钟。")
                      .arg(minutes)
                      .arg(over);
    } else if (goal > 0) {
        const int left = (goal - total) / 60;
        message = QStringLiteral("今日已使用 %1 分钟，距目标还差 %2 分钟。")
                      .arg(minutes)
                      .arg(left);
    } else {
        message = QStringLiteral("今日已使用 %1 分钟。").arg(minutes);
    }

    if (!topApp.isEmpty())
        message += QStringLiteral("主力应用：%1。").arg(topApp);
    message += QStringLiteral("记得起来活动一下～");
    return message;
}

QString ReminderScheduler::reminderTitle() const
{
    return QStringLiteral("Time Master 提醒");
}

// 去除 AI 文本中可能的 Markdown 记号（*、#、-、数字列表等），仅保留纯文本。
QString ReminderScheduler::stripMarkdown(const QString &text)
{
    QString out = text;
    out.remove(QRegularExpression(QStringLiteral("[*#`>|]")));
    out.remove(QRegularExpression(QStringLiteral("^-\\s+"), QRegularExpression::MultilineOption));
    out.replace(QRegularExpression(QStringLiteral("\\[([^\\]]+)\\]\\([^)]*\\)")),
                QStringLiteral("\\1"));
    return out.trimmed();
}
