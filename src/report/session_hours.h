#pragma once

#include <QDate>
#include <QDateTime>

namespace SessionHours {

/// 将会话按小时切开。visitor 收到 (chunkStart, chunkSeconds)。
/// 禁止用 SQL 的 strftime('%H', start_time) 分组，会话会跨小时。
template <typename Fn>
void forEachHourChunk(const QDateTime &start, int secs, Fn &&fn)
{
    if (!start.isValid() || secs <= 0)
        return;
    qint64 remaining = secs;
    QDateTime cursor = start;
    while (remaining > 0) {
        const int intoHour = cursor.time().msecsSinceStartOfDay() % 3600000 / 1000;
        const qint64 chunk = qMin<qint64>(remaining, 3600 - intoHour);
        fn(cursor, int(chunk));
        remaining -= chunk;
        cursor = cursor.addSecs(int(chunk));
    }
}

/// 把会话分摊进某一天的 24 小时桶；跨午夜溢出的部分不计入该日。
inline void addToDayHours(const QDateTime &start, int secs, const QDate &date,
                          int hourTotals[24], int periodSeconds[4] = nullptr)
{
    forEachHourChunk(start, secs, [&](const QDateTime &cursor, int chunk) {
        if (cursor.date() != date)
            return;
        hourTotals[cursor.time().hour()] += chunk;
        if (periodSeconds)
            periodSeconds[cursor.time().hour() / 6] += chunk;
    });
}

} // namespace SessionHours
