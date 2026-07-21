#pragma once

#include <QWidget>

class CompareCard : public QWidget
{
    Q_OBJECT

public:
    explicit CompareCard(QWidget *parent = nullptr);
    void setData(int todaySeconds, int yesterdaySeconds);

    QSize minimumSizeHint() const override { return QSize(200, 140); }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    int m_todaySeconds     = 0;
    int m_yesterdaySeconds = 0;
};
