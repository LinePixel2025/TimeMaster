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

/// Usage trend: bar/line chart (normal) or calendar heatmap, with view toggles.
class TrendCard : public CardFrame
{
    Q_OBJECT
public:
    explicit TrendCard(QWidget *parent = nullptr);

    void setData(const QVector<QVariantMap> &weekData);
    void setMonthData(const QVector<QVariantMap> &monthData);
    void setChartType(const QString &type);
    QString chartType() const;

    /// "normal"（柱状/折线）| "heatmap"（热力图）
    void setDisplayFormat(const QString &format);
    QString displayFormat() const;

    /// 热力图周期："week" | "month"
    void setHeatmapPeriod(const QString &period);
    QString heatmapPeriod() const;

signals:
    void chartTypeChanged(const QString &type);
    void heatmapPeriodChanged(const QString &period);

private:
    QString toggleStyle(QPushButton *btn) const;
    void updateTitle();

    QButtonGroup *m_group = nullptr;
    QButtonGroup *m_heatGroup = nullptr;
    QPushButton *m_barBtn = nullptr;
    QPushButton *m_lineBtn = nullptr;
    QPushButton *m_weekBtn = nullptr;
    QPushButton *m_monthBtn = nullptr;
    TrendChartArea *m_chartArea = nullptr;
};
