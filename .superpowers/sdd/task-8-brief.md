### Task 8: AppRankWidget

**Files:**
- Create: `src/ui/app_rank_widget.h`
- Create: `src/ui/app_rank_widget.cpp`

**Interfaces:**
- Consumes: `DatabaseManager`, Qt6::Widgets
- Produces: `AppRankWidget` showing per-app time ranking

- [ ] **Step 1: Write app_rank_widget.h**

```cpp
#ifndef APP_RANK_WIDGET_H
#define APP_RANK_WIDGET_H

#include <QWidget>
#include <QFrame>
#include <QVBoxLayout>
#include <QLabel>

class DatabaseManager;

class AppRankItem : public QWidget
{
    Q_OBJECT
public:
    AppRankItem(int rank, const QString &appName, int totalSeconds, int maxSeconds,
                QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    int m_rank;
    QString m_appName;
    int m_totalSeconds;
    int m_maxSeconds;
};

class AppRankWidget : public QFrame
{
    Q_OBJECT
public:
    explicit AppRankWidget(DatabaseManager *db, QWidget *parent = nullptr);
    void refresh();

private:
    DatabaseManager *m_db;
    QVBoxLayout *m_listLayout;
    QWidget *m_listWidget;
};

#endif // APP_RANK_WIDGET_H
```

- [ ] **Step 2: Write app_rank_widget.cpp**

```cpp
#include "app_rank_widget.h"
#include "database/database_manager.h"

#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QFont>

AppRankItem::AppRankItem(int rank, const QString &appName, int totalSeconds, int maxSeconds,
                         QWidget *parent)
    : QWidget(parent), m_rank(rank), m_appName(appName),
      m_totalSeconds(totalSeconds), m_maxSeconds(maxSeconds)
{
    setFixedHeight(48);
}

void AppRankItem::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    double w = width();
    double h = height();
    double barMaxW = w - 200;

    painter.setFont(QFont("PingFang SC", 13, QFont::Normal));
    painter.setPen(QColor(255, 255, 255, 200));
    painter.drawText(QRectF(8, 0, 30, h), Qt::AlignLeft | Qt::AlignVCenter, QString::number(m_rank));

    painter.setPen(QColor(255, 255, 255, 220));
    painter.drawText(QRectF(40, 0, 100, h), Qt::AlignLeft | Qt::AlignVCenter, m_appName);

    double ratio = (m_maxSeconds > 0) ? (static_cast<double>(m_totalSeconds) / m_maxSeconds) : 0;
    double barW = barMaxW * ratio;
    if (barW > 0) {
        QLinearGradient gradient(0, 0, barW, 0);
        gradient.setColorAt(0.0, QColor("#818CF8"));
        gradient.setColorAt(1.0, QColor("#6366F1"));
        painter.setBrush(QBrush(gradient));
        painter.setPen(Qt::NoPen);
        QPainterPath path;
        path.addRoundedRect(140, h / 2 - 6, barW, 12, 6, 6);
        painter.drawPath(path);
    }

    int mins = m_totalSeconds / 60;
    int hours = mins / 60;
    int remainMins = mins % 60;
    QString timeStr = (hours > 0) ? QString("%1h %2m").arg(hours).arg(remainMins)
                                  : QString("%1m").arg(remainMins);
    painter.setPen(QColor(255, 255, 255, 140));
    painter.drawText(QRectF(w - 60, 0, 55, h), Qt::AlignRight | Qt::AlignVCenter, timeStr);
}

AppRankWidget::AppRankWidget(DatabaseManager *db, QWidget *parent)
    : QFrame(parent), m_db(db)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(4);

    QLabel *title = new QLabel(QString::fromUtf8("\xe5\xba\x94\xe7\x94\xa8\xe4\xbd\xbf\xe7\x94\xa8\xe6\x8e\x92\xe8\xa1\x8c (\xe4\xbb\x8a\xe6\x97\xa5)"), this);
    title->setFont(QFont("PingFang SC", 14, QFont::Normal));
    title->setStyleSheet("color: rgba(255,255,255,180);");
    layout->addWidget(title);

    m_listWidget = new QWidget(this);
    m_listLayout = new QVBoxLayout(m_listWidget);
    m_listLayout->setContentsMargins(0, 8, 0, 0);
    m_listLayout->setSpacing(2);
    layout->addWidget(m_listWidget);
}

void AppRankWidget::refresh()
{
    QVector<QVariantMap> data = m_db->getAppRank();

    // Clear existing items
    while (m_listLayout->count()) {
        QLayoutItem *item = m_listLayout->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    int maxSec = data.isEmpty() ? 1 : data[0]["total_seconds"].toInt();
    for (int i = 0; i < data.size(); ++i) {
        AppRankItem *rankItem = new AppRankItem(
            i + 1, data[i]["app_name"].toString(),
            data[i]["total_seconds"].toInt(), maxSec, m_listWidget);
        m_listLayout->addWidget(rankItem);
    }
    m_listLayout->addStretch();
}
```

- [ ] **Step 3: Commit**

```bash
git add -A
git commit -m "feat: AppRankWidget with per-app time ranking list"
```

---

