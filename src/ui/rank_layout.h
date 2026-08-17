#pragma once

#include <QString>
#include <QVariantMap>
#include <QVector>

#include <algorithm>

namespace RankLayout {

struct Item
{
    QString appName;
    QString processName;
    int seconds = 0;
    int rank = 0;
    int sharePercent = 0;
    double share = 0;
};

inline QVector<Item> normalize(const QVector<QVariantMap> &rankData)
{
    QVector<Item> items;
    items.reserve(rankData.size());
    for (const QVariantMap &row : rankData) {
        const int seconds = row.value(QStringLiteral("total_seconds")).toInt();
        if (seconds <= 0)
            continue;
        Item item;
        item.appName = row.value(QStringLiteral("app_name")).toString();
        item.processName = row.value(QStringLiteral("process_name")).toString();
        item.seconds = seconds;
        items.append(item);
    }

    std::sort(items.begin(), items.end(), [](const Item &left, const Item &right) {
        if (left.seconds != right.seconds)
            return left.seconds > right.seconds;
        return left.appName.localeAwareCompare(right.appName) < 0;
    });

    int total = 0;
    for (const Item &item : items)
        total += item.seconds;
    for (int index = 0; index < items.size(); ++index) {
        Item &item = items[index];
        item.rank = index + 1;
        item.share = total > 0 ? static_cast<double>(item.seconds) / total : 0.0;
        item.sharePercent = total > 0 ? item.seconds * 100 / total : 0;
    }
    return items;
}

} // namespace RankLayout
