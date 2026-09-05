#pragma once

#include <QDialog>
#include <QStackedWidget>
#include <QListWidget>
#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QSpinBox>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QTimeEdit>
#include <QTimer>

#include "ui/app_manage_page.h"

class DatabaseManager;
class UpdateChecker;
struct UpdateInfo;

class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SettingsDialog(DatabaseManager *db, UpdateChecker *updater,
                            QWidget *parent = nullptr);

    /// 取消/Esc 关闭时还原未保存的主题色预览。
    void reject() override;

protected:
    /// 首次显示时把 Windows 标题栏刷成当前主题底色（winId 此时才有效）。
    void showEvent(QShowEvent *event) override;

signals:
    void settingsChanged();

private slots:
    void onCheckUpdateClicked();
    void onUpdateCheckResult(bool hasUpdate, const UpdateInfo &info,
                             const QString &error);

private:
    void applyTheme();
    void showPage(int index);
    void loadSettings();
    void saveSettings();
    void updateCloudStatus();
    void updateReminderStatus();
    void updateAccentSwatches();
    void fetchGoalFromCloud(const QString &endpoint, const QString &token);
    /// 从更新缓存刷新「关于」页更新状态标签。
    void refreshUpdateStatus();

    DatabaseManager *m_db;

    QStackedWidget *m_stack = nullptr;
    QList<QPushButton *> m_navButtons;
    AppManagePage *m_appManagePage = nullptr;
    QCheckBox *m_trackingEnabled = nullptr;
    QSpinBox *m_pollInterval = nullptr;
    QSpinBox *m_idleThreshold = nullptr;
    QSpinBox *m_minTrackingSeconds = nullptr;
    QSpinBox *m_minRecordThreshold = nullptr;
    QCheckBox *m_autoStart = nullptr;
    QCheckBox *m_darkMode = nullptr;
    QList<QPushButton *> m_accentSwatches;
    QColor m_initialAccent; // 打开对话框时的主题色，取消时据此还原
    QComboBox *m_trendFormat = nullptr;
    QSpinBox *m_dailyGoal = nullptr;

    QCheckBox *m_linewebEnabled = nullptr;
    QLineEdit *m_linewebEndpoint = nullptr;
    QLineEdit *m_linewebToken = nullptr;
    QPushButton *m_linewebTokenToggle = nullptr;
    QSpinBox *m_linewebInterval = nullptr;
    QPushButton *m_linewebTestBtn = nullptr;
    QLabel *m_linewebStatus = nullptr;
    QTimer *m_linewebStatusTimer = nullptr;

    QCheckBox *m_aiEnabled = nullptr;
    QLineEdit *m_aiEndpoint = nullptr;
    QLineEdit *m_aiApiKey = nullptr;
    QPushButton *m_aiApiKeyToggle = nullptr;
    QLineEdit *m_aiModel = nullptr;
    QPushButton *m_aiTestBtn = nullptr;

    QCheckBox *m_reminderEnabled = nullptr;
    QListWidget *m_reminderTimesList = nullptr;
    QTimeEdit *m_reminderTimeEdit = nullptr;
    QPushButton *m_reminderAddBtn = nullptr;
    QPushButton *m_reminderRemoveBtn = nullptr;
    QLabel *m_reminderStatus = nullptr;

    QCheckBox *m_intervalReminderEnabled = nullptr;
    QSpinBox *m_intervalReminderMinutes = nullptr;

    QCheckBox *m_weeklyReportEnabled = nullptr;
    QComboBox *m_weeklyReportDay = nullptr;
    QTimeEdit *m_weeklyReportTime = nullptr;

    UpdateChecker *m_updater = nullptr;
    QPushButton *m_updateCheckBtn = nullptr;
    QLabel *m_updateStatus = nullptr;
};
