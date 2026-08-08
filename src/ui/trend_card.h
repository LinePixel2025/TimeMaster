#pragma once

#include "ui/card_frame.h"

#include <QDate>
#include <QMap>
#include <QPushButton>
#include <QVariantMap>
#include <QVector>

class QButtonGroup;
class QWidget;
class TrendChartArea;

/// Weekly usage trend: bar/line chart with a view toggle.
class TrendCard : public CardFrame
{
    Q_OBJECT
public:
    explicit TrendCard(QWidget *parent = nullptr);

    void setData(const QVector<QVariantMap> &weekData);
    void setChartType(const QString &type);
    QString chartType() const;

signals:
    void chartTypeChanged(const QString &type);

private:
    QString toggleStyle(QPushButton *btn) const;

    QButtonGroup *m_group = nullptr;
    QPushButton *m_barBtn = nullptr;
    QPushButton *m_lineBtn = nullptr;
    TrendChartArea *m_chartArea = nullptr;
};
