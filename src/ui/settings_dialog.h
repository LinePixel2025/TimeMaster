#pragma once

#include <QDialog>
#include <QStackedWidget>
#include <QListWidget>
#include <QTableWidget>
#include <QCheckBox>
#include <QComboBox>
#include <QSpinBox>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QTimeEdit>
#include <QTimer>

class DatabaseManager;

class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SettingsDialog(DatabaseManager *db, QWidget *parent = nullptr);
    ~SettingsDialog() override;

signals:
    void settingsChanged();

private slots:
    void onAddIgnored();
    void onRemoveIgnored();
    void onAddAlias();
    void onEditAlias();
    void onDeleteAlias();
    void filterKnownApps(const QString &text);
    void filterIgnoredApps(const QString &text);

private:
    void applyTheme();
    void showPage(int index);
    void loadSettings();
    void saveSettings();
    void updateCloudStatus();
    void updateReminderStatus();
    void fetchGoalFromCloud(const QString &endpoint, const QString &token);
    void refreshKnownAppsList();
    void refreshIgnoredList();
    void refreshAliasTable();

    DatabaseManager *m_db;

    QStackedWidget *m_stack = nullptr;
    QList<QPushButton *> m_navButtons;
    QString m_prevAppStyleSheet;
    QListWidget *m_knownAppsList = nullptr;
    QListWidget *m_ignoredAppsList = nullptr;
    QTableWidget *m_aliasTable = nullptr;
    QCheckBox *m_trackingEnabled = nullptr;
    QSpinBox *m_pollInterval = nullptr;
    QSpinBox *m_idleThreshold = nullptr;
    QSpinBox *m_minTrackingSeconds = nullptr;
    QSpinBox *m_minRecordThreshold = nullptr;
    QCheckBox *m_autoStart = nullptr;
    QCheckBox *m_darkMode = nullptr;
    QComboBox *m_trendFormat = nullptr;
    QSpinBox *m_dailyGoal = nullptr;
    QLineEdit *m_knownSearch = nullptr;
    QLineEdit *m_ignoredSearch = nullptr;

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
};
