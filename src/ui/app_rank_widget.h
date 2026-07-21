#ifndef APP_RANK_WIDGET_H
#define APP_RANK_WIDGET_H

#include <QWidget>
#include <QFrame>
#include <QVBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QIcon>

#include "rank_card.h"

class DatabaseManager;

class AppRankWidget : public QFrame
{
    Q_OBJECT
public:
    explicit AppRankWidget(DatabaseManager *db, QWidget *parent = nullptr);
    void refresh();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    DatabaseManager *m_db;
    QVBoxLayout *m_listLayout;
    QWidget *m_listWidget;
    QScrollArea *m_scrollArea;
};

#endif // APP_RANK_WIDGET_H
