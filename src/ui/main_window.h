#pragma once

#include <QMainWindow>
#include <QTimer>

class DatabaseManager;
class QPushButton;
class QLabel;

class HeroCard;
class TrendCard;
class RankCard;
class CompareCard;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(DatabaseManager *db, QWidget *parent = nullptr);

public slots:
    void refreshData();

signals:
    void settingsChanged();

protected:
    void showEvent(QShowEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onExport();
    void onSettings();

private:
    DatabaseManager *m_db = nullptr;
    QTimer *m_refreshTimer = nullptr;
    QPushButton *m_themeBtn = nullptr;
    QPushButton *m_settingsBtn = nullptr;
    QPushButton *m_exportBtn = nullptr;
    QPushButton *m_refreshBtn = nullptr;
    HeroCard *m_heroCard = nullptr;
    TrendCard *m_trendCard = nullptr;
    RankCard *m_rankCard = nullptr;
    CompareCard *m_compareCard = nullptr;
};
