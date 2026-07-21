#ifndef APP_ICON_PROVIDER_H
#define APP_ICON_PROVIDER_H

#include <QIcon>
#include <QHash>
#include <QMutex>

class AppIconProvider
{
public:
    static AppIconProvider* instance();
    QIcon icon(const QString &processPath, int size = 24);

private:
    AppIconProvider();
    QIcon extractIcon(const QString &processPath, int size);
    void createFallbackIcon();

    QMutex m_mutex;
    QHash<QString, QIcon> m_cache;
    QIcon m_fallbackIcon;
};

#endif // APP_ICON_PROVIDER_H
