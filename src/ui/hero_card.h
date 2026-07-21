#pragma once
#include <QFrame>
#include <QVector>
#include <QVariantMap>
class QLabel;

class HeroCard : public QFrame
{
    Q_OBJECT
public:
    explicit HeroCard(QWidget *parent = nullptr);
    void setData(int todaySec, int yesterdaySec, int goalSec, const QVector<QVariantMap> &);
    QSize minimumSizeHint() const override { return QSize(400, 280); }
signals: void clicked();
protected: void mousePressEvent(QMouseEvent *e) override;
private: void updateDisplay();
    int m_today=0, m_yesterday=0, m_goal=0;
    QLabel *m_time=nullptr, *m_sub=nullptr, *m_ring=nullptr;
};
