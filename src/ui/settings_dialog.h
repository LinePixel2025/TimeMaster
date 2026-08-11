#pragma once

#include <QDialog>
#include <QTabWidget>
#include <QListWidget>
#include <QTableWidget>
#include <QCheckBox>
#include <QSpinBox>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QTimer>

class DatabaseManager;

class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SettingsDialog(DatabaseManager *db, QWidget *parent = nullptr);

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
    void loadSettings();
    void saveSettings();
    void updateCloudStatus();
    void fetchGoalFromCloud(const QString &endpoint, const QString &token);
    void refreshKnownAppsList();
    void refreshIgnoredList();
    void refreshAliasTable();

    DatabaseManager *m_db;

    QTabWidget *m_tabWidget = nullptr;
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
};
