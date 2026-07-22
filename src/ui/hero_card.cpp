#include "ui/hero_card.h"
#include "ui/design_tokens.h"
#include "ui/theme_manager.h"
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QDate>

static QString f(int s){int m=qMax(0,s)/60,h=m/60;m%=60;return h>0?QString("%1h%2m").arg(h).arg(m,2,10,QChar('0')):QString("%1m").arg(m);}

HeroCard::HeroCard(QWidget *p):QFrame(p){
    setMinimumHeight(220);
    setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Minimum);
    setStyleSheet("HeroCard{background:transparent;}");

    auto *L=new QVBoxLayout(this);L->setContentsMargins(0,8,0,8);L->setSpacing(0);
    auto *R=new QHBoxLayout();
    auto *d=new QLabel(QStringLiteral("TODAY \u00B7 ")+QDate::currentDate().toString("yyyy-MM-dd"),this);
    d->setFont(DesignTokens::eyebrowFont(11));
    d->setStyleSheet(QString("color:%1;background:transparent;").arg(DesignTokens::kTextFaint().name()));
    R->addWidget(d);R->addStretch();L->addLayout(R);L->addSpacing(12);

    m_time=new QLabel("0m",this);m_time->setFont(DesignTokens::appFont(44,QFont::Bold));
    m_time->setStyleSheet(QString("color:%1;background:transparent;").arg(DesignTokens::kTextStrong().name()));
    L->addWidget(m_time);L->addSpacing(4);

    m_sub=new QLabel(QString::fromUtf8("\xe4\xbb\x8a\xe6\x97\xa5\xe6\x80\xbb\xe6\x97\xb6\xe9\x95\xbf"),this);
    m_sub->setFont(DesignTokens::appFont(14));
    m_sub->setStyleSheet(QString("color:%1;background:transparent;").arg(DesignTokens::kTextMute().name()));
    L->addWidget(m_sub);

    L->addStretch();
    m_ring=new QLabel(this);m_ring->setFont(DesignTokens::appFont(18,QFont::Medium));
    m_ring->setStyleSheet(QString("color:%1;padding:8px 16px;background:%2;border-radius:16px;")
        .arg(DesignTokens::kAccent().name(),DesignTokens::kAccentLight().name()));
    m_ring->setAlignment(Qt::AlignCenter);L->addWidget(m_ring,0,Qt::AlignRight);

    connect(ThemeManager::instance(), &ThemeManager::themeChanged,
            this, [this](ThemeManager::Theme) {
        QLabel* d = findChild<QLabel*>();
        if (d) d->setStyleSheet(QString("color:%1;background:transparent;").arg(DesignTokens::kTextFaint().name()));
        m_time->setStyleSheet(QString("color:%1;background:transparent;").arg(DesignTokens::kTextStrong().name()));
        m_sub->setStyleSheet(QString("color:%1;background:transparent;").arg(DesignTokens::kTextMute().name()));
        m_ring->setStyleSheet(QString("color:%1;padding:8px 16px;background:%2;border-radius:16px;")
            .arg(DesignTokens::kAccent().name(), DesignTokens::kAccentLight().name()));
    });
}
void HeroCard::setData(int t,int y,int g,const QVector<QVariantMap>&){m_today=t;m_yesterday=y;m_goal=g;updateDisplay();}
void HeroCard::updateDisplay(){
    QString s=f(m_today);if(m_goal>0)s+=QString("  /%1h").arg(qMax(0,m_goal)/3600);m_time->setText(s);
    QString b=QString::fromUtf8("\xe4\xbb\x8a\xe6\x97\xa5\xe6\x80\xbb\xe6\x97\xb6\xe9\x95\xbf");
    if(m_yesterday>0){int d=(int)((m_today-m_yesterday)*100.0/m_yesterday);if(d>0)b+=QString("  \u2191%1%").arg(d);else if(d<0)b+=QString("  \u2193%1%").arg(-d);else b+="  \u21920%";}
    m_sub->setText(b);
    m_ring->setText(QString("%1%").arg(m_goal>0?qMin(100,m_today*100/m_goal):0));
}
void HeroCard::mousePressEvent(QMouseEvent *e){QFrame::mousePressEvent(e);if(e->button()==Qt::LeftButton)emit clicked();}
