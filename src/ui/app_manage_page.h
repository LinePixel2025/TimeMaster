#pragma once

#include <QWidget>
#include <QVector>

#include "database/database_manager.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class DatabaseManager;

/// 设置「应用管理」页：左侧已追踪应用列表，右侧选中应用的属性面板
/// （显示名 / 组别 / 合并 / 屏蔽），下方组别管理区。
/// 与设置对话框其它页面一致，改动即时落库，不参与「保存/取消」事务。
class AppManagePage : public QWidget
{
    Q_OBJECT
public:
    explicit AppManagePage(DatabaseManager *db, QWidget *parent = nullptr);

    /// 外部（设置对话框）切换进本页或追踪配置变化时调用，重新拉取数据。
    void reload();

signals:
    /// 屏蔽/别名/组别/合并任一变更：通知主窗口刷新并让追踪层重载配置。
    void appsChanged();

private slots:
    void filterApps(const QString &text);
    void onAppSelected(QListWidgetItem *current, QListWidgetItem *previous);
    void onRenameClicked();
    void onGroupChanged(int index);
    void onMergeChanged(int index);
    void onIgnoreToggled(bool checked);
    void onAddGroup();
    void onRenameGroup();
    void onRemoveGroup();

private:
    /// 页内 QSS：只作用于本页控件，避免污染对话框其它页面。
    QString pageStyle() const;
    void applyIcons();
    void refreshList();
    void refreshGroups();
    void refreshDetails();
    void updateGroupActions();
    /// 当前选中应用的进程键；未选中时返回空串。
    QString currentKey() const;
    /// 「合并到」下拉项携带的目标键；索引 0 为「不合并」。
    QString mergeTargetFor(int index) const;

    DatabaseManager *m_db = nullptr;

    QLineEdit *m_search = nullptr;
    QListWidget *m_appList = nullptr;
    QLabel *m_detailTitle = nullptr;
    QLabel *m_detailStats = nullptr;
    QLabel *m_detailHint = nullptr;
    QPushButton *m_renameBtn = nullptr;
    QComboBox *m_groupCombo = nullptr;
    QComboBox *m_mergeCombo = nullptr;
    QCheckBox *m_ignoreCheck = nullptr;
    QListWidget *m_groupList = nullptr;
    QPushButton *m_renameGroupBtn = nullptr;
    QPushButton *m_removeGroupBtn = nullptr;

    QVector<AppEntry> m_apps;
    QVector<QVariantMap> m_groups;
    /// 屏蔽列表 id -> 进程键，切换开关时需要按 id 删除。
    QMap<QString, int> m_ignoredIds;
    bool m_updating = false; // 回显填充控件期间抑制槽函数写库
};
