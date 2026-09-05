#pragma once

#include <QWidget>
#include <QVector>

#include "database/database_manager.h"

class QCheckBox;
class QComboBox;
class QDialog;
class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class DatabaseManager;
class AiClient;

/// 设置「应用管理」页：应用列表是本页主角（左侧 hero 卡片，按累计时长降序、
/// 占满整页高度）；右列上方为选中应用的属性面板（显示名 / 组别 / 合并 / 屏蔽），
/// 下方为紧凑的组别管理卡片。
/// 以 PID 形式记录（取不到可执行文件路径）的应用折叠为列表末尾的
/// 「未识别应用」行，点击弹窗查看清单；选中其中某个应用后可在右侧
/// 改名或用 AI 根据窗口标题识别应用名。
/// 组别行带 emoji 图标（app_groups.icon）；点选组别会把左侧应用列表
/// 筛选为该组组员（✕ 芯片退出）。累计时长不足 2 分钟的应用默认隐藏，
/// 由工具栏「隐藏低时长应用」开关控制（仅影响列表展示，不改数据）。
/// 与设置对话框其它页面一致，改动即时落库，不参与「保存/取消」事务。
class AppManagePage : public QWidget
{
    Q_OBJECT
public:
    explicit AppManagePage(DatabaseManager *db, AiClient *ai,
                           QWidget *parent = nullptr);

    /// 外部（设置对话框）切换进本页或追踪配置变化时调用，重新拉取数据。
    void reload();

signals:
    /// 屏蔽/别名/组别/合并任一变更：通知主窗口刷新并让追踪层重载配置。
    void appsChanged();

private slots:
    void filterApps(const QString &text);
    void onAppSelected(QListWidgetItem *current, QListWidgetItem *previous);
    /// 折叠行点击：打开/前置「未识别应用」弹窗。
    void onAppRowClicked(QListWidgetItem *item);
    void onRenameClicked();
    void onGroupChanged(int index);
    void onMergeChanged(int index);
    void onIgnoreToggled(bool checked);
    void onAddGroup();
    void onRenameGroup();
    void onRemoveGroup();
    /// 组别列表选中变化：同步重命名/删除按钮与「只看该组」筛选。
    void onGroupSelectionChanged();
    void clearGroupFilter();
    void onHideLowUsageToggled(bool checked);
    /// AI 识别选中 PID 应用（结果经确认后才写别名）。
    void onIdentifyClicked();
    void onIdentifyReady(const QString &tag, const QString &name);
    void onIdentifyFailed(const QString &tag, const QString &error);

private:
    /// 页内 QSS：只作用于本页控件，避免污染对话框其它页面。
    QString pageStyle() const;
    void applyIcons();
    void refreshList();
    void refreshGroups();
    void refreshDetails();
    /// 弹窗已创建时同步其内容（列表重建后调用）。
    void refreshPidDialog();
    void ensurePidDialog();
    /// 详情面板当前应用键；未选中时返回空串。可能指向弹窗内的 PID 应用。
    QString currentKey() const;
    /// 「合并到」下拉项携带的目标键；索引 0 为「不合并」。
    QString mergeTargetFor(int index) const;
    /// 恢复 AI 识别按钮为可用文案。
    void resetIdentifyButton();

    DatabaseManager *m_db = nullptr;
    AiClient *m_ai = nullptr;

    QLineEdit *m_search = nullptr;
    QListWidget *m_appList = nullptr;
    QCheckBox *m_hideLowUsage = nullptr;
    QPushButton *m_groupFilterChip = nullptr;
    QLabel *m_detailTitle = nullptr;
    QLabel *m_detailStats = nullptr;
    QLabel *m_detailHint = nullptr;
    QPushButton *m_renameBtn = nullptr;
    QPushButton *m_identifyBtn = nullptr;
    QComboBox *m_groupCombo = nullptr;
    QComboBox *m_mergeCombo = nullptr;
    QCheckBox *m_ignoreCheck = nullptr;
    QListWidget *m_groupList = nullptr;
    QPushButton *m_renameGroupBtn = nullptr;
    QPushButton *m_removeGroupBtn = nullptr;

    // 「未识别应用」弹窗（非模态，按需创建，父为本页）。
    QDialog *m_pidDialog = nullptr;
    QListWidget *m_pidList = nullptr;

    int m_groupFilterId = -1; // 「只看该组」筛选的组别 id；-1 为不筛选
    QString m_currentKey;     // 右侧详情面板展示的应用键
    QString m_aiPendingKey;   // 正在 AI 识别的应用键；非空时忽略新请求
    QVector<AppEntry> m_apps;
    QVector<AppEntry> m_pidApps; // m_apps 中 PID 形式条目的子集
    QVector<QVariantMap> m_groups;
    bool m_updating = false; // 回显填充控件期间抑制槽函数写库
};
