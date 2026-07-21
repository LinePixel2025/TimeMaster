#pragma once

#include <QWidget>
#include <QStackedWidget>
#include <QButtonGroup>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QVector>
#include <QVariantMap>
#include <QMap>
#include <QDate>
#include <QStringList>

// ========== WeeklyBar ==========
class WeeklyBar : public QWidget
{
    Q_OBJECT

public:
    explicit WeeklyBar(QWidget *parent = nullptr);
    void setData(const QVector<QVariantMap> &weekData);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QMap<QString, int> m_data;
    int m_maxVal = 1;
};

// ========== WeeklyLine ==========
class WeeklyLine : public QWidget
{
    Q_OBJECT

public:
    explicit WeeklyLine(QWidget *parent = nullptr);
    void setData(const QVector<QVariantMap> &weekData);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QMap<QString, int> m_data;
    int m_maxVal = 1;
};

// ========== ChartCard ==========
class ChartCard : public QWidget
{
    Q_OBJECT

public:
    explicit ChartCard(QWidget *parent = nullptr);

    /// Feed week data to both chart widgets.
    void setData(const QVector<QVariantMap> &weekData);

    /// Set initial chart type ("bar" or "line").
    void setChartType(const QString &type);

signals:
    /// Emitted when user clicks bar/line toggle.
    /// Parent should persist via DatabaseManager::setSetting("chart_type", type).
    void chartTypeChanged(const QString &type);

private:
    void setupUi();
    void applyToggleStyle(QPushButton *btn, bool checked);

    QStackedWidget *m_stack;
    WeeklyBar *m_bar;
    WeeklyLine *m_line;
    QButtonGroup *m_chartGroup;
    QPushButton *m_barBtn;
    QPushButton *m_lineBtn;
};
