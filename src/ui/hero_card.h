#pragma once

#include "ui/card_frame.h"

#include <array>

class QBoxLayout;
class QLabel;
class QResizeEvent;
class PeriodDistributionArea;
class QWidget;

/// 今日总时长：汇总时长、同比、目标与四时段分布。
class HeroCard : public CardFrame
{
    Q_OBJECT
public:
    explicit HeroCard(QWidget *parent = nullptr);

    void setData(int todaySeconds, int yesterdaySeconds, int goalSeconds,
                 const std::array<int, 24> &hourTotals);

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void updateDisplay();
    void applyLabelColors();
    void updateCompactLayout();

    int m_today = 0;
    int m_yesterday = 0;
    int m_goal = 0;
    QWidget *m_summaryArea = nullptr;
    QBoxLayout *m_bodyLayout = nullptr;
    QLabel *m_timeLabel = nullptr;
    QLabel *m_comparisonLabel = nullptr;
    QLabel *m_goalLabel = nullptr;
    PeriodDistributionArea *m_periodDistribution = nullptr;
    bool m_compactLayout = false;
};
