#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QDialog>

class DatabaseManager;
class DashboardWidget;

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
    void setupPalette();

    DatabaseManager *m_db;
    DashboardWidget *m_dashboardWidget;
    QTimer *m_refreshTimer;
    QWidget *m_centralWidget;
};

#endif // MAIN_WINDOW_H
