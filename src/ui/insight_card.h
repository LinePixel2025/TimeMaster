#ifndef INSIGHT_CARD_H
#define INSIGHT_CARD_H

#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QStackedWidget>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>

class InsightCard : public QWidget
{
    Q_OBJECT
public:
    explicit InsightCard(QWidget *parent = nullptr);

    void setConfigured(bool configured);
    void setLoading(bool loading);
    void setError(const QString &error);
    void setResult(const QString &headline, const QString &summary,
                   const QString &suggestions);

signals:
    void requestAnalysis();
    void viewFullReport();

private:
    QStackedWidget *m_stack = nullptr;

    // Page 0 widgets
    QWidget *m_idlePage = nullptr;
    QLabel *m_notConfiguredLabel = nullptr;
    QPushButton *m_analyzeBtn = nullptr;

    // Page 2 widgets
    QLabel *m_errorLabel = nullptr;
    QPushButton *m_retryBtn = nullptr;

    // Page 3 widgets
    QLabel *m_headlineLabel = nullptr;
    QLabel *m_summaryLabel = nullptr;
    QPushButton *m_reportBtn = nullptr;
};

#endif // INSIGHT_CARD_H
