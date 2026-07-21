#ifndef STATS_WIDGET_H
#define STATS_WIDGET_H

#include <QWidget>
#include <QFrame>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QMap>
#include <QDate>
#include <QIcon>
#include <QStackedWidget>

#include "chart_card.h"
#include "topapp_card.h"

class DatabaseManager;

class BigNumberWidget : public QWidget
{
    Q_OBJECT
public:
    explicit BigNumberWidget(QWidget *parent = nullptr);
    void setValue(int totalSeconds);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    int m_value = 0;
    static constexpr int m_maxValue = 43200;
    int m_hours = 0;
    int m_minutes = 0;
};

class GlassCard : public QFrame
{
    Q_OBJECT
public:
    explicit GlassCard(QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;
};

class YesterdayCompare : public QWidget
{
    Q_OBJECT
public:
    explicit YesterdayCompare(QWidget *parent = nullptr);
    void setData(int todaySeconds, int yesterdaySeconds);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    int m_todaySeconds = 0;
    int m_yesterdaySeconds = 0;
};

class StatsWidget : public QWidget
{
    Q_OBJECT
public:
    explicit StatsWidget(DatabaseManager *db, QWidget *parent = nullptr);
    void refresh();

private:
    DatabaseManager *m_db;
    BigNumberWidget *m_bigNumber;
    WeeklyBar *m_weeklyBar;
    WeeklyLine *m_weeklyLine;
    QStackedWidget *m_chartStack;
    TopAppCard *m_topAppCard;
    YesterdayCompare *m_yesterdayCompare;
};

#endif // STATS_WIDGET_H
