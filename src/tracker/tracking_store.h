#ifndef TRACKING_STORE_H
#define TRACKING_STORE_H

#include <QDateTime>
#include <QString>

class TrackingStore
{
public:
    virtual ~TrackingStore() = default;

    virtual qint64 insertSession(const QString &processName, const QString &windowTitle,
                                 const QString &appName, const QDateTime &startTime,
                                 const QDateTime &endTime, int durationSeconds) = 0;
    virtual bool updateSessionEnd(qint64 sessionId, const QDateTime &endTime,
                                  int durationSeconds) = 0;
    virtual bool updateSessionDuration(qint64 sessionId, int durationSeconds) = 0;
};

#endif // TRACKING_STORE_H
