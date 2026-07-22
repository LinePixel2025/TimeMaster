#pragma once
#include <QFrame>
#include <QLabel>
#include <QVBoxLayout>

class DashboardCard : public QFrame
{
    Q_OBJECT
public:
    explicit DashboardCard(const QString &cardId, QWidget *parent = nullptr);
    QString cardId() const { return m_cardId; }
    void setCardTitle(const QString &title);
    void setContentWidget(QWidget *w);
    QSize minimumSizeHint() const override;

signals:
    void clicked();
protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *e) override;
private:
    void updateClipMask();

    QString m_cardId;
    QLabel *m_titleLabel = nullptr;
    QWidget *m_contentWidget = nullptr;
    QVBoxLayout *m_layout;
};
