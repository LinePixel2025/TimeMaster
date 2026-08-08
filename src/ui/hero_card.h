#pragma once

#include "ui/card_frame.h"

class QLabel;
class GoalRing;

/// Today's total time: big number, goal ring, and day-over-day change.
class HeroCard : public CardFrame
{
    Q_OBJECT
public:
    explicit HeroCard(QWidget *parent = nullptr);

    void setData(int todaySeconds, int yesterdaySeconds, int goalSeconds);

private:
    void updateDisplay();

    int m_today = 0;
    int m_yesterday = 0;
    int m_goal = 0;
    QLabel *m_timeLabel = nullptr;
    QLabel *m_subLabel = nullptr;
    QLabel *m_goalLabel = nullptr;
    GoalRing *m_ring = nullptr;
};
