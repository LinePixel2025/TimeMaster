#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QDialog>
#include <QMap>
#include <QGridLayout>

class DatabaseManager;
class QPushButton;
class QLabel;
class QWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(DatabaseManager *db, QWidget *parent = nullptr);

public slots:
    void refreshData();
    void loadLayout();

protected:
    void showEvent(QShowEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onExport();
    void onSettings();

signals:
    void settingsChanged();

private:
    QWidget *createCard(const QString &cardId);
    void clearLayout();

    DatabaseManager *m_db;
    QTimer *m_refreshTimer;
    QWidget *m_centralWidget;
    QPushButton *m_themeBtn;
    QLabel *m_titleLabel;
    QPushButton *m_settingsBtn;
    QPushButton *m_exportBtn;
    QPushButton *m_refreshBtn;
    QGridLayout *m_contentGrid;
    QMap<QString, QWidget*> m_cards;
};

#endif // MAIN_WINDOW_H
