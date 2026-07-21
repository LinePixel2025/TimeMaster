#pragma once

#include <QString>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QVector>
#include <QStringList>
#include <QMap>

/// Represents one dashboard component's visibility and grid position.
/// row=-1 signals "not specified" — backward compat assigns default.
struct DashboardLayoutItem {
    QString id;            // Widget identifier (e.g. "today_total")
    bool visible = true;   // Whether to show this component
    int row = -1;          // Grid row (-1 = auto-assign from default mapping)
    int col = 0;           // Grid column (0 or 1 for 2-column grid)
    int colSpan = 1;       // Column span (1 or 2 for full-width)

    bool operator==(const DashboardLayoutItem &other) const {
        return id == other.id && visible == other.visible
            && row == other.row && col == other.col && colSpan == other.colSpan;
    }
};

Q_DECLARE_METATYPE(DashboardLayoutItem)

/// Parser and serializer for the dashboard_layout JSON setting.
/// JSON format: [{"id":"today_total","visible":true}, ...]
/// Array order = display order (top to bottom).
namespace DashboardLayoutParser {

    /// List of all valid widget IDs in canonical default order.
    QStringList validIds();

    /// Default grid positions for the redesigned dashboard:
    /// hero full-width top → balanced 2-column insight grid → comparison footer.
    QMap<QString, QVector<int>> defaultGrid();

    /// Assign default grid position if row == -1 (old format backward compat).
    void assignDefaultPosition(DashboardLayoutItem &item);

    /// Return the canonical default layout: all 6 components visible, in canonical order.
    QVector<DashboardLayoutItem> defaultLayout();

    /// Parse JSON string into layout items.
    /// - Empty string → defaultLayout()
    /// - Invalid JSON → defaultLayout() (graceful degradation)
    /// - Unknown IDs → silently filtered out
    /// - Duplicate IDs → last occurrence wins
    /// - Missing "visible" field → defaults to true
    QVector<DashboardLayoutItem> parse(const QString &json);

    /// Serialize layout items to JSON string.
    QString serialize(const QVector<DashboardLayoutItem> &items);

    /// Ensure all valid IDs are present. Missing IDs are appended at the end,
    /// preserving existing order for present IDs.
    QVector<DashboardLayoutItem> ensureComplete(const QVector<DashboardLayoutItem> &items);

} // namespace DashboardLayoutParser
