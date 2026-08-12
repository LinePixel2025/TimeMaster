#include "tray_manager.h"
#include "ui/design_tokens.h"
#include "ui/theme_manager.h"
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

    applyMenuTheme();
    connect(ThemeManager::instance(), &ThemeManager::themeChanged,
            this, [this]() { applyMenuTheme(); });

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

void TrayManager::applyMenuTheme()
{
    m_menu->setStyleSheet(QString(
        "QMenu { background-color: %1; color: %2; border: 1px solid %3; padding: 4px; }"
        "QMenu::item { padding: 6px 24px 6px 12px; border-radius: 4px; }"
        "QMenu::item:selected { background-color: %4; color: %5; }"
        "QMenu::separator { height: 1px; background: %3; margin: 4px 8px; }")
        .arg(DesignTokens::kSurface().name(),
             DesignTokens::kTextStrong().name(),
             DesignTokens::kBorder().name(),
             DesignTokens::kAccentLight().name(),
             DesignTokens::kTextStrong().name()));
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
    // 使用正式信息图标（而非空图标），部分 Windows 版本对空图标气泡显示不可靠。
    m_tray->showMessage(title, message, QSystemTrayIcon::Information, 3000);
}

void TrayManager::onActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::DoubleClick)
        emit showMainWindow();
}
