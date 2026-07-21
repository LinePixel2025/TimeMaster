#include "tray_manager.h"
#include <QCoreApplication>
#include <QFile>
#include <QDir>
#include <QAction>

TrayManager::TrayManager(const QString &appName, QObject *parent)
    : QObject(parent)
{
    m_tray = new QSystemTrayIcon(this);
    m_tray->setIcon(loadIcon());
    m_tray->setToolTip(appName);

    m_menu = new QMenu();
    QAction *showAction = m_menu->addAction(QString::fromUtf8("\xe6\x98\xbe\xe7\xa4\xba\xe4\xb8\xbb\xe7\x95\x8c\xe9\x9d\xa2"));
    connect(showAction, &QAction::triggered, this, &TrayManager::showMainWindow);

    m_menu->addSeparator();

    QAction *quitAction = m_menu->addAction(QString::fromUtf8("\xe9\x80\x80\xe5\x87\xba"));
    connect(quitAction, &QAction::triggered, this, &TrayManager::quitApp);

    m_tray->setContextMenu(m_menu);
    connect(m_tray, &QSystemTrayIcon::activated, this, &TrayManager::onActivated);
}

QIcon TrayManager::loadIcon()
{
    QString iconPath = ":/icon.png";
    if (!QFile::exists(iconPath))
        iconPath = QCoreApplication::applicationDirPath() + "/icon.png";
    if (QFile::exists(iconPath))
        return QIcon(iconPath);
    return QIcon();
}

void TrayManager::show()
{
    m_tray->show();
}

void TrayManager::setTooltip(const QString &text)
{
    m_tray->setToolTip(text);
}

void TrayManager::showNotification(const QString &title, const QString &message)
{
    m_tray->showMessage(title, message, QIcon(), 3000);
}

void TrayManager::onActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::DoubleClick)
        emit showMainWindow();
}
