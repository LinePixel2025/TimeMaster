#ifndef RANK_CARD_H
#define RANK_CARD_H

#include <QFrame>
#include <QIcon>
#include <QScrollArea>
#include <QString>
#include <QVBoxLayout>
#include <QVector>
#include <QVariantMap>
#include <QWidget>

class AppRankItem : public QWidget
{
    Q_OBJECT
public:
    AppRankItem(int rank, const QString &appName, int totalSeconds,
                const QIcon &icon, QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    int m_rank;
    QString m_appName;
    int m_totalSeconds;
    QIcon m_icon;
};

class RankCard : public QFrame
{
    Q_OBJECT
public:
    explicit RankCard(QWidget *parent = nullptr);
    void refresh(const QVector<QVariantMap> &rankData);

private:
    QVBoxLayout *m_listLayout = nullptr;
    QWidget *m_listWidget = nullptr;
};

#endif // RANK_CARD_H
