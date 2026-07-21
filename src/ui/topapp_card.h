#pragma once

#include <QWidget>
#include <QLabel>
#include <QIcon>

class TopAppCard : public QWidget
{
    Q_OBJECT

public:
    explicit TopAppCard(QWidget *parent = nullptr);
    void setApp(const QString &name, int seconds, const QIcon &icon = QIcon());

private:
    QLabel *m_eyebrowLabel = nullptr;
    QLabel *m_iconLabel    = nullptr;
    QLabel *m_nameLabel    = nullptr;
    QLabel *m_timeLabel    = nullptr;
};
