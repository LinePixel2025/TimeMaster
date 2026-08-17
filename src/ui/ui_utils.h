#pragma once

#include <QMenu>
#include <QString>

#include "ui/design_tokens.h"

namespace UiUtils {

/// Format seconds as "5h 32m" / "45m" / "0m".
inline QString formatDuration(int totalSeconds)
{
    const int minutes = qMax(0, totalSeconds) / 60;
    const int hours = minutes / 60;
    const int remMins = minutes % 60;
    if (hours > 0)
        return QStringLiteral("%1h %2m").arg(hours).arg(remMins);
    return QStringLiteral("%1m").arg(minutes);
}

/// Compact form used in tight spaces: "1h46" (no "m" suffix).
inline QString formatCompact(int totalSeconds)
{
    const int minutes = qMax(0, totalSeconds) / 60;
    const int hours = minutes / 60;
    const int remMins = minutes % 60;
    if (hours > 0)
        return QStringLiteral("%1h%2").arg(hours).arg(remMins, 2, 10, QLatin1Char('0'));
    return QStringLiteral("%1m").arg(minutes);
}

/// 弹出菜单统一走设计 token，避免 Windows 原生菜单在暗色下黑底黑字。
inline void applyMenuStyle(QMenu *menu)
{
    if (!menu)
        return;
    menu->setStyleSheet(QStringLiteral(
        "QMenu { background: %1; color: %2; border: 1px solid %3;"
        " border-radius: 8px; padding: 6px 2px; }"
        "QMenu::item { padding: 7px 26px 7px 16px; border-radius: 6px; }"
        "QMenu::item:selected { background: %4; color: %5; }"
        "QMenu::item:disabled { color: %6; }"
        "QMenu::separator { height: 1px; background: %3; margin: 5px 8px; }")
        .arg(DesignTokens::kSurface().name(),
             DesignTokens::kText().name(),
             DesignTokens::kBorder().name(),
             DesignTokens::kAccentLight().name(),
             DesignTokens::kAccent().name(),
             DesignTokens::kTextFaint().name()));
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
