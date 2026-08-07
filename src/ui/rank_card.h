#pragma once

#include "ui/card_frame.h"

#include <QIcon>
#include <QScrollArea>
#include <QVariantMap>
#include <QVector>

class QVBoxLayout;

/// Scrollable app usage ranking list.
class RankCard : public CardFrame
{
    Q_OBJECT
public:
    explicit RankCard(QWidget *parent = nullptr);

    void refresh(const QVector<QVariantMap> &rankData);

private:
    QVBoxLayout *m_listLayout = nullptr;
    QWidget *m_listWidget = nullptr;
    QScrollArea *m_scrollArea = nullptr;
};
