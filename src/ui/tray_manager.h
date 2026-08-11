#ifndef TRAY_MANAGER_H
#define TRAY_MANAGER_H

#include <QObject>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QIcon>

class TrayManager : public QObject
{
    Q_OBJECT
public:
    explicit TrayManager(const QString &appName = "Time Master", QObject *parent = nullptr);
    void show();
    void setTooltip(const QString &text);
    void showNotification(const QString &title, const QString &message);

signals:
    void showMainWindow();
    void quitApp();

private slots:
    void onActivated(QSystemTrayIcon::ActivationReason reason);

private:
    QIcon loadIcon();
    void applyMenuTheme();
    QSystemTrayIcon *m_tray;
    QMenu *m_menu;
};

#endif // TRAY_MANAGER_H
