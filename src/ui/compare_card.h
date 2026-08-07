#pragma once

#include "ui/card_frame.h"

/// Today vs yesterday comparison bars.
class CompareCard : public CardFrame
{
    Q_OBJECT
public:
    explicit CompareCard(QWidget *parent = nullptr);

    void setData(int todaySeconds, int yesterdaySeconds);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    int m_today = 0;
    int m_yesterday = 0;
};
