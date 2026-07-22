#include "ui/dashboard_layout.h"

namespace DashboardLayoutParser {

QStringList validIds()
{
    return {
        "today_total",
        "weekly_chart",
        "ai_insight",
        "top_app",
        "app_ranking",
        "yesterday_compare"
    };
}

QMap<QString, QVector<int>> defaultGrid()
{
    return {
        {"today_total",       {0, 0, 2}},
        {"weekly_chart",      {1, 0, 1}},
        {"ai_insight",        {1, 1, 1}},
        {"top_app",           {2, 0, 1}},
        {"app_ranking",       {2, 1, 1}},
        {"yesterday_compare", {3, 0, 2}},
    };
}

void assignDefaultPosition(DashboardLayoutItem &item)
{
    auto grid = defaultGrid();
    if (grid.contains(item.id)) {
        auto pos = grid[item.id];
        item.row = pos[0];
        item.col = pos[1];
        item.colSpan = pos[2];
    } else {
        item.row = 0;
        item.col = 0;
        item.colSpan = 1;
    }
}

QVector<DashboardLayoutItem> defaultLayout()
{
    QVector<DashboardLayoutItem> items;
    for (const auto &id : validIds()) {
        DashboardLayoutItem item;
        item.id = id;
        item.visible = true;
        assignDefaultPosition(item);
        items.append(item);
    }
    return items;
}

QVector<DashboardLayoutItem> parse(const QString &json)
{
    if (json.trimmed().isEmpty()) {
        return defaultLayout();
    }

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isArray()) {
        return defaultLayout();
    }

    QJsonArray arr = doc.array();
    QVector<DashboardLayoutItem> items;
    QStringList valid = validIds();

    for (const auto &val : arr) {
        if (!val.isObject()) continue;

        QJsonObject obj = val.toObject();
        QString id = obj.value("id").toString();
        if (id.isEmpty()) continue;

        // Validate against known IDs
        if (!valid.contains(id)) continue;

        // Dedup: remove previous occurrence of this ID (last wins)
        for (int i = 0; i < items.size(); ++i) {
            if (items[i].id == id) {
                items.removeAt(i);
                break;
            }
        }

        DashboardLayoutItem item;
        item.id = id;
        // Missing "visible" field defaults to true
        if (obj.contains("visible"))
            item.visible = obj.value("visible").toBool();
        if (obj.contains("row"))
            item.row = obj.value("row").toInt(-1);
        if (obj.contains("col"))
            item.col = obj.value("col").toInt(0);
        if (obj.contains("colSpan"))
            item.colSpan = obj.value("colSpan").toInt(1);
        items.append(item);
    }

    // When the parsed layout has at least 4 items (indicating a full layout
    // rather than an editor partial selection), ensure all valid IDs are present
    // for backward compatibility (e.g., new cards added in a newer version).
    if (items.size() >= 4) {
        items = ensureComplete(items);
    }

    // Backward compatibility: assign default grid positions if missing
    for (auto &item : items) {
        if (item.row < 0) {
            assignDefaultPosition(item);
        } else {
            // Clamp invalid values
            if (item.col < 0) item.col = 0;
            if (item.col > 1) item.col = 1;
            if (item.colSpan < 1) item.colSpan = 1;
            if (item.colSpan > 2) item.colSpan = 2;
            if (item.col + item.colSpan > 2) item.colSpan = 2 - item.col;
        }
    }

    return items;
}

QString serialize(const QVector<DashboardLayoutItem> &items)
{
    QJsonArray arr;
    for (const auto &item : items) {
        QJsonObject obj;
        obj["id"] = item.id;
        obj["visible"] = item.visible;
        obj["row"] = item.row;
        obj["col"] = item.col;
        obj["colSpan"] = item.colSpan;
        arr.append(obj);
    }
    QJsonDocument doc(arr);
    return QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
}

QVector<DashboardLayoutItem> ensureComplete(const QVector<DashboardLayoutItem> &items)
{
    QVector<DashboardLayoutItem> result = items;
    QStringList presentIds;
    for (const auto &item : result) {
        presentIds.append(item.id);
    }
    for (const auto &id : validIds()) {
        if (!presentIds.contains(id)) {
            DashboardLayoutItem missing;
            missing.id = id;
            missing.visible = true;
            assignDefaultPosition(missing);
            result.append(missing);
        }
    }
    return result;
}

} // namespace DashboardLayoutParser
