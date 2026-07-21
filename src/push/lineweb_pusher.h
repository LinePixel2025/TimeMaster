#ifndef LINEWEB_PUSHER_H
#define LINEWEB_PUSHER_H

#include <QObject>
#include <QString>
#include <QTimer>
#include <QNetworkAccessManager>

class DatabaseManager;

class LineWebPusher : public QObject
{
    Q_OBJECT
public:
    explicit LineWebPusher(DatabaseManager *db, QObject *parent = nullptr);

    void start();
    void stop();
    void pushNow();
    void reloadSettings();

signals:
    void pushSucceeded();
    void pushFailed(const QString &error);

private:
    void doPush();

    DatabaseManager *m_db;
    QNetworkAccessManager *m_nam;
    QTimer *m_timer;
    QString m_token;
    QString m_endpoint;
    int m_intervalMinutes = 10;
    bool m_enabled = false;
};

#endif // LINEWEB_PUSHER_H
