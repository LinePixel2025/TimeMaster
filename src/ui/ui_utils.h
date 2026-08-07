#pragma once

#include <QString>

namespace UiUtils {

/// Format seconds as "5h32m" / "45m" / "0m".
inline QString formatDuration(int totalSeconds)
{
    const int minutes = qMax(0, totalSeconds) / 60;
    const int hours = minutes / 60;
    const int remMins = minutes % 60;
    if (hours > 0)
        return QString("%1h%2m").arg(hours).arg(remMins);
    return QString("%1m").arg(minutes);
}

/// Compact form used in tight spaces: "1h46" (no "m" suffix).
inline QString formatCompact(int totalSeconds)
{
    const int minutes = qMax(0, totalSeconds) / 60;
    const int hours = minutes / 60;
    const int remMins = minutes % 60;
    if (hours > 0)
        return QString("%1h%2").arg(hours).arg(remMins, 2, 10, QLatin1Char('0'));
    return QString("%1m").arg(minutes);
}

/// Percentage change between today and yesterday (rounded).
inline int percentChange(int todaySeconds, int yesterdaySeconds)
{
    if (yesterdaySeconds <= 0)
        return 0;
    return static_cast<int>((todaySeconds - yesterdaySeconds) * 100.0 / yesterdaySeconds);
}

/// Convert a process path (e.g. "C:\\Program Files\\Foo\\app.exe") to a display name.
inline QString friendlyAppName(const QString &processPath)
{
    int pos = processPath.lastIndexOf(QLatin1Char('\\'));
    QString name = (pos >= 0) ? processPath.mid(pos + 1) : processPath;
    if (name.endsWith(QLatin1String(".exe"), Qt::CaseInsensitive))
        name.chop(4);
    if (!name.isEmpty())
        name[0] = name[0].toUpper();
    return name;
}

} // namespace UiUtils
