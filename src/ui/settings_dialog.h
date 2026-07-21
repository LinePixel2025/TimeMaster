#ifndef SETTINGS_DIALOG_H
#define SETTINGS_DIALOG_H

#include <QDialog>
#include <QTabWidget>
#include <QListWidget>
#include <QTableWidget>
#include <QCheckBox>
#include <QSpinBox>
#include <QPushButton>
#include <QLineEdit>
#include <QColor>
#include <QLabel>

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
    void refreshKnownAppsList();
    void refreshIgnoredList();
    void refreshAliasTable();

    DatabaseManager *m_db;

    QTabWidget *m_tabWidget;
    QListWidget *m_knownAppsList;
    QListWidget *m_ignoredAppsList;
    QTableWidget *m_aliasTable;
    QCheckBox *m_trackingEnabled;
    QSpinBox *m_pollInterval;
    QSpinBox *m_idleThreshold;
    QSpinBox *m_minTrackingSeconds;
    QSpinBox *m_minRecordThreshold;
    QCheckBox *m_autoStart;
    QLineEdit *m_knownSearch;
    QLineEdit *m_ignoredSearch;

    QCheckBox *m_linewebEnabled;
    QLineEdit *m_linewebEndpoint;
    QLineEdit *m_linewebToken;
    QPushButton *m_linewebTokenToggle;
    QSpinBox *m_linewebInterval;
    QPushButton *m_linewebTestBtn;
    QLabel *m_linewebStatus;
};

#endif // SETTINGS_DIALOG_H
