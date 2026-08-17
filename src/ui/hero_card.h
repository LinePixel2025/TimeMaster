#pragma once

#include "ui/card_frame.h"

#include <array>

class QLabel;
class SundialArea;

/// 今日总时长：大数字、昨日对比、目标进度，以及 24 小时日晷。
class HeroCard : public CardFrame
{
    Q_OBJECT
public:
    explicit HeroCard(QWidget *parent = nullptr);

    void setData(int todaySeconds, int yesterdaySeconds, int goalSeconds,
                 const std::array<int, 24> &hourTotals);

private:
    void updateDisplay();
    void applyLabelColors();

    int m_today = 0;
    int m_yesterday = 0;
    int m_goal = 0;
    QLabel *m_timeLabel = nullptr;
    QLabel *m_subLabel = nullptr;
    SundialArea *m_sundial = nullptr;
};
