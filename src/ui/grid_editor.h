#pragma once

#include <QWidget>
#include <QStackedWidget>
#include <QLabel>
#include <QPushButton>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDropEvent>
#include <QVector>
#include <QMap>
#include <QStringList>
#include <QListWidget>

#include "ui/dashboard_layout.h"

// ============================================================================
// CellWidget — one grid cell in the dashboard editor
// ============================================================================
// QStackedWidget-based (2 pages). No deleteLater() in event handlers.
// Page 0: placeholder label (dashed border, empty state)
// Page 1: component name + × remove button (solid border, occupied state)
// Accepts drops via application/x-qabstractitemmodeldatalist MIME type.
class CellWidget : public QWidget
{
    Q_OBJECT
public:
    explicit CellWidget(int row, int col, QWidget *parent = nullptr);

    /// Place a component into this cell. Switches to page 1.
    void setComponent(const QString &id, const QString &displayName, int colSpan = 1);

    /// Clear this cell. Switches to page 0.
    void clearComponent();

    /// Update visual styling based on state changes or theme switch.
    void updateStyle();

    bool hasComponent() const { return !m_componentId.isEmpty(); }
    QString componentId() const { return m_componentId; }
    int colSpan() const { return m_colSpan; }

    int  row() const { return m_row; }
    int  col() const { return m_col; }

    /// Update internal row index after row removal.
    void setRow(int row) { m_row = row; }

signals:
    /// Emitted when the × button is clicked (the cell has already been cleared).
    void removeClicked(const QString &id);

    /// Emitted when a component is dropped onto this cell.
    void componentDropped(const QString &id, const QString &displayName);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    int m_row;
    int m_col;
    int m_colSpan = 1;
    QString m_componentId;
    bool m_hovering = false;

    QStackedWidget *m_stack;
    QLabel         *m_placeholder;
    QLabel         *m_nameLabel;
    QPushButton    *m_removeBtn;
};


// ============================================================================
// DashboardGridEditor — 2-column drag-and-drop grid editor
// ============================================================================
// Manages a QGridLayout of CellWidgets. Supports add/remove rows, drag-drop
// placement, duplicate detection, and hidden-item tracking.
class DashboardGridEditor : public QWidget
{
    Q_OBJECT
public:
    explicit DashboardGridEditor(QWidget *parent = nullptr);

    /// Populate the grid from parsed layout items.
    /// nameMap: component id → display name
    /// iconMap: component id → resource path (stored for updateLibraryStates)
    void setLayoutItems(const QVector<DashboardLayoutItem> &items,
                        const QMap<QString, QString> &nameMap,
                        const QMap<QString, QString> &iconMap);

    /// Serialize current grid state back to layout items.
    QVector<DashboardLayoutItem> layoutItems() const;

    /// Attach the component library list widget (for drag states).
    void setLibrary(QListWidget *library) { m_library = library; }

    /// Gray out library items that are already placed in the grid.
    void updateLibraryStates();

    int  rowCount() const { return m_cells.size(); }
    bool isDirty() const  { return m_dirty; }

    /// Set number of rows (adds/removes from end).
    void setRows(int count);

signals:
    void layoutChanged();

public slots:
    /// Append a new empty row at the bottom.
    void addRow();

    /// Remove the last row.
    void removeLastRow();

    /// Handle a component drop onto a cell.
    void onCellDrop(const QString &id, const QString &displayName);

private:
    /// Ensure at least `count` rows exist (create or trim from end).
    void ensureRows(int count);

    /// Remove the row at `index` and re-number cells below.
    void removeRow(int index);

    /// Return the CellWidget at (row, col), or nullptr.
    CellWidget *cellAt(int row, int col);

    /// If the component `id` already exists in another cell, clear it.
    void clearDuplicate(CellWidget *except, const QString &id);

    /// Re-add all cells of a row to the grid layout with correct spans.
    void relayoutRow(int row);

    QGridLayout *m_gridLayout;
    QPushButton *m_addRowBtn;
    QPushButton *m_removeRowBtn;
    QListWidget *m_library = nullptr;

    QMap<QString, QString> m_nameMap;
    QMap<QString, QString> m_iconMap;
    QVector<QVector<CellWidget*>> m_cells;

    QStringList m_hiddenIds;
    bool m_dirty = false;
};
