#pragma once

#include "ui/card_frame.h"

class QWidget;
class CompareArea;

/// Today vs yesterday comparison bars.
class CompareCard : public CardFrame
{
    Q_OBJECT
public:
    explicit CompareCard(QWidget *parent = nullptr);

    void setData(int todaySeconds, int yesterdaySeconds);

private:
    CompareArea *m_area = nullptr;
};
