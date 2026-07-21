#include "app_rank_widget.h"
#include "database/database_manager.h"
#include "icon/app_icon_provider.h"

#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QFont>

static QFont appFont(int size, QFont::Weight weight = QFont::Normal)
{
    QFont font("Microsoft YaHei", size, weight);
    font.setStyleStrategy(QFont::PreferAntialias);
    return font;
}

AppRankWidget::AppRankWidget(DatabaseManager *db, QWidget *parent)
    : QFrame(parent), m_db(db)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    QLabel *title = new QLabel(QString::fromUtf8("\xe5\xba\x94\xe7\x94\xa8\xe4\xbd\xbf\xe7\x94\xa8\xe6\x8e\x92\xe8\xa1\x8c (\xe4\xbb\x8a\xe6\x97\xa5)"), this);
    title->setFont(appFont(14, QFont::Medium));
    title->setStyleSheet("color: #1F2937;");
    layout->addWidget(title);

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setStyleSheet("QScrollArea { background: transparent; border: none; }");

    m_listWidget = new QWidget();
    m_listWidget->setStyleSheet("background: transparent;");
    m_listLayout = new QVBoxLayout(m_listWidget);
    m_listLayout->setContentsMargins(0, 0, 0, 0);
    m_listLayout->setSpacing(8);
    m_listLayout->addStretch();

    m_scrollArea->setWidget(m_listWidget);
    layout->addWidget(m_scrollArea);
}

void AppRankWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QPainterPath path;
    path.addRoundedRect(0, 0, width(), height(), 16, 16);
    QLinearGradient gradient(0, 0, 0, height());
    gradient.setColorAt(0.0, QColor(255, 255, 255, 190));
    gradient.setColorAt(1.0, QColor(255, 255, 255, 140));
    painter.fillPath(path, QBrush(gradient));
    QPen pen(QColor(255, 255, 255, 180), 1);
    painter.setPen(pen);
    painter.drawPath(path);
}

void AppRankWidget::refresh()
{
    QVector<QVariantMap> data = m_db->getAppRank();

    // Clear existing items (keep stretch at the end)
    while (m_listLayout->count() > 1) {
        QLayoutItem *item = m_listLayout->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    for (int i = 0; i < data.size(); ++i) {
        QString processName = data[i]["process_name"].toString();
        QIcon icon = AppIconProvider::instance()->icon(processName, 24);
        AppRankItem *rankItem = new AppRankItem(
            i + 1, data[i]["app_name"].toString(),
            data[i]["total_seconds"].toInt(), icon, m_listWidget);
        m_listLayout->insertWidget(i, rankItem);
    }
}
