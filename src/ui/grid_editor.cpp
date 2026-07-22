#include "ui/grid_editor.h"
#include "ui/design_tokens.h"

#include <QDataStream>
#include <QMimeData>
#include <QSet>

// ============================================================================
// Style helpers
// ============================================================================
static const char *kStyleEmpty  =
    "background: transparent;"
    "border: 2px dashed %1;"
    "border-radius: 8px;";

static const char *kStyleOccupied =
    "background: %1;"
    "border: 2px solid %2;"
    "border-radius: 8px;";

static const char *kStyleHover =
    "background: %1;"
    "border: 2px dashed %2;"
    "border-radius: 8px;";

// ============================================================================
// CellWidget
// ============================================================================

CellWidget::CellWidget(int row, int col, QWidget *parent)
    : QWidget(parent)
    , m_row(row)
    , m_col(col)
    , m_componentId()
{
    setAcceptDrops(true);
    setMinimumHeight(72);
    setMinimumWidth(120);

    // ---- QStackedWidget — 2 pages ----
    m_stack = new QStackedWidget(this);
    m_stack->setContentsMargins(0, 0, 0, 0);

    // Page 0: placeholder
    m_placeholder = new QLabel(
        QString::fromUtf8("\xe6\x8b\x96\xe6\x94\xbe\xe7\xbb\x84\xe4\xbb\xb6\xe5\x88\xb0\xe6\xad\xa4\xe5\xa4\x84"),
        this);
    m_placeholder->setAlignment(Qt::AlignCenter);
    m_placeholder->setFont(DesignTokens::appFont(11));
    m_placeholder->setStyleSheet(
        QStringLiteral("color: %1; background: transparent;")
            .arg(DesignTokens::kTextFaint().name(QColor::HexArgb)));
    m_stack->addWidget(m_placeholder); // index 0

    // Page 1: component display
    QWidget *contentPage = new QWidget(this);
    QHBoxLayout *contentLayout = new QHBoxLayout(contentPage);
    contentLayout->setContentsMargins(10, 4, 4, 4);
    contentLayout->setSpacing(4);

    m_nameLabel = new QLabel(this);
    m_nameLabel->setFont(DesignTokens::appFont(12, QFont::Medium));
    m_nameLabel->setStyleSheet(
        QStringLiteral("color: %1; background: transparent;")
            .arg(DesignTokens::kAccent().name(QColor::HexArgb)));
    m_nameLabel->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    contentLayout->addWidget(m_nameLabel, 1);

    m_removeBtn = new QPushButton(
        QString::fromUtf8("\xc3\x97"), this); // multiplication sign ×
    m_removeBtn->setFixedSize(24, 24);
    m_removeBtn->setCursor(Qt::PointingHandCursor);
    m_removeBtn->setFont(DesignTokens::appFont(14, QFont::Bold));
    m_removeBtn->setStyleSheet(
        QStringLiteral(
            "QPushButton {"
            "  background: transparent;"
            "  border: none;"
            "  color: %1;"
            "  border-radius: 12px;"
            "}"
            "QPushButton:hover {"
            "  background: %2;"
            "  color: %3;"
            "}")
        .arg(DesignTokens::kTextFaint().name(QColor::HexArgb))
        .arg(DesignTokens::kError().name(QColor::HexArgb))
        .arg(DesignTokens::kSurface().name(QColor::HexArgb)));
    contentLayout->addWidget(m_removeBtn);

    m_stack->addWidget(contentPage); // index 1

    // Main layout
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(2, 2, 2, 2);
    mainLayout->addWidget(m_stack);

    // ---- Connections ----
    // × button: save id before clearComponent, then emit removeClicked.
    // NO deleteLater() — pure page switch.
    connect(m_removeBtn, &QPushButton::clicked, this, [this]() {
        if (m_componentId.isEmpty())
            return;
        QString id = m_componentId;
        clearComponent();
        emit removeClicked(id);
    });

    // Initial visual state
    clearComponent();
}

void CellWidget::setComponent(const QString &id, const QString &displayName, int colSpan)
{
    m_componentId = id;
    m_colSpan = qBound(1, colSpan, 2);
    m_nameLabel->setText(displayName);
    m_stack->setCurrentIndex(1);
    updateStyle();
}

void CellWidget::clearComponent()
{
    m_componentId.clear();
    m_colSpan = 1;
    m_nameLabel->clear();
    m_stack->setCurrentIndex(0);
    updateStyle();
}

// ---- Drag & drop -----------------------------------------------------------

void CellWidget::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasFormat("application/x-qabstractitemmodeldatalist")) {
        event->acceptProposedAction();
        m_hovering = true;
        updateStyle();
    }
}

void CellWidget::dragLeaveEvent(QDragLeaveEvent *event)
{
    Q_UNUSED(event);
    m_hovering = false;
    updateStyle();
}

void CellWidget::dropEvent(QDropEvent *event)
{
    m_hovering = false;
    updateStyle();

    if (!event->mimeData()->hasFormat("application/x-qabstractitemmodeldatalist"))
        return;

    QByteArray data = event->mimeData()->data("application/x-qabstractitemmodeldatalist");
    QDataStream stream(&data, QIODevice::ReadOnly);

    if (stream.atEnd())
        return;

    int row = -1;
    int col = -1;
    QMap<int, QVariant> roleDataMap;
    stream >> row >> col >> roleDataMap;

    QString id = roleDataMap.value(Qt::UserRole).toString();
    QString displayName = roleDataMap.value(Qt::DisplayRole).toString();

    if (id.isEmpty())
        return;

    event->acceptProposedAction();
    emit componentDropped(id, displayName);
}

// ---- Private ---------------------------------------------------------------

void CellWidget::updateStyle()
{
    QString borderColor = DesignTokens::kBorder().name(QColor::HexArgb);
    QString accentColor = DesignTokens::kAccent().name(QColor::HexArgb);
    QString accentAlpha = DesignTokens::kAccentLight().name(QColor::HexArgb);

    if (m_hovering) {
        setStyleSheet(QString::fromLatin1(kStyleHover)
            .arg(accentAlpha, DesignTokens::kAccent().name(QColor::HexArgb)));
    } else if (m_stack->currentIndex() == 1) {
        setStyleSheet(QString::fromLatin1(kStyleOccupied)
            .arg(accentAlpha, accentColor));
    } else {
        setStyleSheet(QString::fromLatin1(kStyleEmpty).arg(borderColor));
    }
}

// ============================================================================
// DashboardGridEditor
// ============================================================================

DashboardGridEditor::DashboardGridEditor(QWidget *parent)
    : QWidget(parent)
{
    QVBoxLayout *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(10);

    // ---- Grid ----
    m_gridLayout = new QGridLayout();
    m_gridLayout->setSpacing(14);
    m_gridLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->addLayout(m_gridLayout, 1);

    // ---- Row control buttons ----
    QHBoxLayout *btnRow = new QHBoxLayout();
    btnRow->setSpacing(8);
    btnRow->addStretch();

    m_removeRowBtn = new QPushButton(
        QString::fromUtf8("\xe2\x88\x92 \xe5\x88\xa0\xe9\x99\xa4\xe6\x9c\x80\xe5\x90\x8e\xe4\xb8\x80\xe8\xa1\x8c"),
        this);
    m_removeRowBtn->setFont(DesignTokens::appFont(11));
    m_removeRowBtn->setStyleSheet(
        QStringLiteral(
            "QPushButton {"
            "  background: %1;"
            "  color: %2;"
            "  border: 1px solid %3;"
            "  border-radius: %4px;"
            "  padding: 5px 12px;"
            "  font-size: 11px;"
            "}"
            "QPushButton:hover {"
            "  background: %5;"
            "}")
        .arg(DesignTokens::kSurface().name(QColor::HexArgb))
        .arg(DesignTokens::kError().name(QColor::HexArgb))
        .arg(DesignTokens::kBorder().name(QColor::HexArgb))
        .arg(DesignTokens::kRadiusBtn)
        .arg(DesignTokens::kAccentGlow().name(QColor::HexArgb)));
    btnRow->addWidget(m_removeRowBtn);

    m_addRowBtn = new QPushButton(
        QString::fromUtf8("+ \xe6\x96\xb0\xe5\xa2\x9e\xe8\xa1\x8c"), this);
    m_addRowBtn->setFont(DesignTokens::appFont(11));
    m_addRowBtn->setStyleSheet(
        QStringLiteral(
            "QPushButton {"
            "  background-color: %1;"
            "  color: white;"
            "  border: none;"
            "  border-radius: %2px;"
            "  padding: 5px 14px;"
            "  font-size: 11px;"
            "}"
            "QPushButton:hover {"
            "  background-color: %3;"
            "}"
            "QPushButton:pressed {"
            "  background-color: %4;"
            "}")
        .arg(DesignTokens::kAccent().name(QColor::HexArgb))
        .arg(DesignTokens::kRadiusBtn)
        .arg(DesignTokens::kAccentHover().name(QColor::HexArgb))
        .arg(DesignTokens::kAccentPressed().name(QColor::HexArgb)));
    btnRow->addWidget(m_addRowBtn);

    outerLayout->addLayout(btnRow);

    // ---- Connections ----
    connect(m_addRowBtn, &QPushButton::clicked, this, &DashboardGridEditor::addRow);
    connect(m_removeRowBtn, &QPushButton::clicked, this, &DashboardGridEditor::removeLastRow);
}

// ---- Public API ------------------------------------------------------------

void DashboardGridEditor::setLayoutItems(const QVector<DashboardLayoutItem> &items,
                                          const QMap<QString, QString> &nameMap,
                                          const QMap<QString, QString> &iconMap)
{
    m_nameMap = nameMap;
    m_iconMap = iconMap;
    m_hiddenIds.clear();

    // Calculate required rows (max row index + 1)
    int maxRow = 0;
    for (const auto &item : items) {
        if (item.visible && item.row > maxRow)
            maxRow = item.row;
    }
    // Also store hidden IDs
    for (const auto &item : items) {
        if (!item.visible)
            m_hiddenIds.append(item.id);
    }

    ensureRows(maxRow + 1);

    // Place visible components
    for (const auto &item : items) {
        if (!item.visible)
            continue;
        CellWidget *cell = cellAt(item.row, item.col);
        if (cell) {
            QString displayName = m_nameMap.value(item.id, item.id);
            cell->setComponent(item.id, displayName, item.colSpan);
        }
    }

    // Apply column spans to the grid layout
    for (int r = 0; r < m_cells.size(); ++r)
        relayoutRow(r);

    // Gray out placed items in the library
    updateLibraryStates();

    m_dirty = false;
}

QVector<DashboardLayoutItem> DashboardGridEditor::layoutItems() const
{
    QVector<DashboardLayoutItem> result;
    for (int r = 0; r < m_cells.size(); ++r) {
        for (int c = 0; c < m_cells[r].size(); ++c) {
            CellWidget *cell = m_cells[r][c];
            if (!cell || !cell->hasComponent())
                continue;
            // Skip col-1 component if col-0 in the same row spans 2 cols
            if (c == 1 && m_cells[r][0] && m_cells[r][0]->colSpan() > 1)
                continue;

            DashboardLayoutItem item;
            item.id = cell->componentId();
            item.visible = true;
            item.row = r;
            item.col = c;
            item.colSpan = cell->colSpan();
            result.append(item);
        }
    }

    // Append hidden items
    for (const QString &hiddenId : m_hiddenIds) {
        DashboardLayoutItem item;
        item.id = hiddenId;
        item.visible = false;
        item.row = 0;
        item.col = 0;
        result.append(item);
    }

    return result;
}

void DashboardGridEditor::updateLibraryStates()
{
    if (!m_library)
        return;

    // Collect all placed component IDs
    QSet<QString> placed;
    for (int r = 0; r < m_cells.size(); ++r) {
        for (int c = 0; c < m_cells[r].size(); ++c) {
            CellWidget *cell = m_cells[r][c];
            if (cell && cell->hasComponent())
                placed.insert(cell->componentId());
        }
    }

    for (int i = 0; i < m_library->count(); ++i) {
        QListWidgetItem *libItem = m_library->item(i);
        QString id = libItem->data(Qt::UserRole).toString();
        if (placed.contains(id)) {
            libItem->setFlags(libItem->flags() & ~Qt::ItemIsEnabled);
            libItem->setForeground(DesignTokens::kTextFaint());
        } else {
            libItem->setFlags(libItem->flags() | Qt::ItemIsEnabled);
            libItem->setForeground(DesignTokens::kText());
        }
    }
}

// ---- Slots ----------------------------------------------------------------

void DashboardGridEditor::addRow()
{
    int newRow = m_cells.size();
    ensureRows(newRow + 1);
    m_dirty = true;
    emit layoutChanged();
}

void DashboardGridEditor::removeLastRow()
{
    if (m_cells.isEmpty())
        return;
    removeRow(m_cells.size() - 1);
    m_dirty = true;
    emit layoutChanged();
}

void DashboardGridEditor::onCellDrop(const QString &id, const QString &displayName)
{
    // Find which cell this drop was on — the sender is the CellWidget
    CellWidget *cell = qobject_cast<CellWidget*>(sender());
    if (!cell)
        return;

    // If this cell already has the same component, nothing to do
    if (cell->componentId() == id)
        return;

    // Clear any duplicate elsewhere
    clearDuplicate(cell, id);

    // Place the component on this cell
    cell->setComponent(id, displayName, 1);
    relayoutRow(cell->row());

    m_dirty = true;
    updateLibraryStates();
    emit layoutChanged();
}

// ---- Private helpers -------------------------------------------------------

void DashboardGridEditor::ensureRows(int count)
{
    // Trim excess rows
    while (m_cells.size() > count)
        removeRow(m_cells.size() - 1);

    // Add missing rows
    while (m_cells.size() < count) {
        int newRow = m_cells.size();
        QVector<CellWidget*> rowCells;
        rowCells.reserve(2);

        for (int c = 0; c < 2; ++c) {
            CellWidget *cell = new CellWidget(newRow, c, this);

            // Wire signals
            connect(cell, &CellWidget::componentDropped,
                    this, &DashboardGridEditor::onCellDrop);
            connect(cell, &CellWidget::removeClicked, this,
                [this, cell](const QString &/*id*/) {
                    // CellWidget::clearComponent() was already called by the × button.
                    // We just handle the grid-level consequences.
                    relayoutRow(cell->row());
                    m_dirty = true;
                    updateLibraryStates();
                    emit layoutChanged();
                });

            m_gridLayout->addWidget(cell, newRow, c, 1, 1);
            rowCells.append(cell);
        }

        m_cells.append(rowCells);
    }
}

void DashboardGridEditor::removeRow(int index)
{
    if (index < 0 || index >= m_cells.size())
        return;

    // Remove from grid layout and delete
    for (int c = 0; c < m_cells[index].size(); ++c) {
        CellWidget *cell = m_cells[index][c];
        m_gridLayout->removeWidget(cell);
        cell->deleteLater();
    }
    m_cells.remove(index);

    // Re-number rows below
    for (int r = index; r < m_cells.size(); ++r) {
        for (int c = 0; c < m_cells[r].size(); ++c) {
            CellWidget *cell = m_cells[r][c];
            cell->setRow(r);
            // Remove and re-add to grid layout at the new row position
            m_gridLayout->removeWidget(cell);
            m_gridLayout->addWidget(cell, r, c, 1, 1);
        }
    }
}

CellWidget *DashboardGridEditor::cellAt(int row, int col)
{
    if (row < 0 || row >= m_cells.size())
        return nullptr;
    if (col < 0 || col >= m_cells[row].size())
        return nullptr;
    return m_cells[row][col];
}

void DashboardGridEditor::clearDuplicate(CellWidget *except, const QString &id)
{
    for (int r = 0; r < m_cells.size(); ++r) {
        for (int c = 0; c < m_cells[r].size(); ++c) {
            CellWidget *cell = m_cells[r][c];
            if (cell && cell != except && cell->componentId() == id) {
                cell->clearComponent();
                relayoutRow(cell->row());
                return; // only one duplicate possible
            }
        }
    }
}

void DashboardGridEditor::relayoutRow(int row)
{
    if (row < 0 || row >= m_cells.size())
        return;

    // Remove both cells from grid layout
    for (int c = 0; c < m_cells[row].size(); ++c) {
        m_gridLayout->removeWidget(m_cells[row][c]);
        m_cells[row][c]->show();
    }

    // Re-add with proper column spans
    CellWidget *cell0 = m_cells[row][0];
    CellWidget *cell1 = m_cells[row][1];

    if (cell0->colSpan() > 1) {
        // col-0 spans both columns; hide col-1
        m_gridLayout->addWidget(cell0, row, 0, 1, 2);
        cell1->hide();
    } else {
        // Normal 2-column layout
        m_gridLayout->addWidget(cell0, row, 0, 1, 1);
        m_gridLayout->addWidget(cell1, row, 1, 1, 1);
    }
}
