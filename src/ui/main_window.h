#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QDialog>

class DatabaseManager;
class StatsWidget;
class AppRankWidget;
class QPushButton;
class QLabel;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(DatabaseManager *db, QWidget *parent = nullptr);

public slots:
    void refreshData();

protected:
    void showEvent(QShowEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onExport();
    void onSettings();

signals:
    void settingsChanged();

private:
    DatabaseManager *m_db;
    StatsWidget *m_statsWidget;
    AppRankWidget *m_appRankWidget;
    QTimer *m_refreshTimer;
    QWidget *m_centralWidget;
    QPushButton *m_themeBtn;
    QLabel *m_titleLabel;
    QPushButton *m_settingsBtn;
    QPushButton *m_exportBtn;
    QPushButton *m_refreshBtn;
};

#endif // MAIN_WINDOW_H
