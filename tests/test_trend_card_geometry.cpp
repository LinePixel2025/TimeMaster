#include <cassert>
#include <iostream>

#include <QApplication>
#include <QPushButton>
#include <QWidget>

#include "ui/theme_manager.h"
#include "ui/trend_card.h"

namespace {

void processEvents()
{
    QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
}

bool intersects(const QWidget *left, const QWidget *right)
{
    const QRect leftRect(left->mapToGlobal(QPoint(0, 0)), left->size());
    const QRect rightRect(right->mapToGlobal(QPoint(0, 0)), right->size());
    return leftRect.intersects(rightRect);
}

void test_trend_card_keeps_header_controls_separate()
{
    QWidget host;
    host.resize(920, 600);
    auto *card = new TrendCard(&host);
    card->setGeometry(0, 0, 900, 360);
    card->show();
    host.show();
    processEvents();

    auto *format = card->findChild<QPushButton *>(QStringLiteral("trendFormatButton"));
    auto *bar = card->findChild<QPushButton *>(QStringLiteral("trendBarButton"));
    auto *line = card->findChild<QPushButton *>(QStringLiteral("trendLineButton"));
    auto *week = card->findChild<QPushButton *>(QStringLiteral("trendWeekButton"));
    auto *month = card->findChild<QPushButton *>(QStringLiteral("trendMonthButton"));
    assert(format && bar && line && week && month);
    assert(format->isVisible() && bar->isVisible() && line->isVisible());
    assert(!week->isVisible() && !month->isVisible());
    assert(!intersects(format, bar));
    assert(!intersects(bar, line));
    std::cout << "test_trend_card_keeps_header_controls_separate PASS" << std::endl;
}

void test_heatmap_uses_available_card_height()
{
    QWidget host;
    host.resize(1000, 900);
    auto *card = new TrendCard(&host);
    card->setDisplayFormat(QStringLiteral("heatmap"));
    card->setHeatmapPeriod(QStringLiteral("month"));
    card->setGeometry(0, 0, 760, 420);
    card->show();
    host.show();
    processEvents();

    auto *chart = card->findChild<QWidget *>(QStringLiteral("trendChartArea"));
    assert(chart);
    const int shortHeight = chart->height();
    assert(card->sizePolicy().verticalPolicy() == QSizePolicy::Expanding);
    assert(chart->sizePolicy().verticalPolicy() == QSizePolicy::Expanding);

    card->setGeometry(0, 0, 760, 620);
    processEvents();
    assert(chart->height() > shortHeight + 150);
    std::cout << "test_heatmap_uses_available_card_height PASS" << std::endl;
}

void test_heatmap_period_switch_keeps_outer_geometry_stable()
{
    QWidget host;
    host.resize(920, 600);
    auto *card = new TrendCard(&host);
    card->setDisplayFormat(QStringLiteral("heatmap"));
    QVector<QVariantMap> monthData;
    const QDate today = QDate::currentDate();
    for (int day = 1; day <= today.day(); ++day) {
        monthData.append({
            {QStringLiteral("d"), QDate(today.year(), today.month(), day).toString(Qt::ISODate)},
            {QStringLiteral("total_seconds"), (day % 5 + 1) * 2700}
        });
    }
    card->setMonthData(monthData);
    card->setGeometry(0, 0, 900, 400);
    card->show();
    host.show();
    processEvents();

    auto *chart = card->findChild<QWidget *>(QStringLiteral("trendChartArea"));
    assert(chart);
    const QSize cardSize = card->size();
    const QRect weekGeometry = chart->geometry();
    card->setHeatmapPeriod(QStringLiteral("month"));
    processEvents();
    assert(card->size() == cardSize);
    assert(chart->geometry() == weekGeometry);
    std::cout << "test_heatmap_period_switch_keeps_outer_geometry_stable PASS" << std::endl;
}

void test_heatmap_mode_shows_only_period_controls()
{
    QWidget host;
    host.resize(920, 600);
    auto *card = new TrendCard(&host);
    card->setDisplayFormat(QStringLiteral("heatmap"));
    card->setHeatmapPeriod(QStringLiteral("month"));
    card->setGeometry(0, 0, 900, 400);
    card->show();
    host.show();
    processEvents();

    auto *bar = card->findChild<QPushButton *>(QStringLiteral("trendBarButton"));
    auto *line = card->findChild<QPushButton *>(QStringLiteral("trendLineButton"));
    auto *week = card->findChild<QPushButton *>(QStringLiteral("trendWeekButton"));
    auto *month = card->findChild<QPushButton *>(QStringLiteral("trendMonthButton"));
    assert(bar && line && week && month);
    assert(!bar->isVisible() && !line->isVisible());
    assert(week->isVisible() && month->isVisible());
    assert(!intersects(week, month));
    std::cout << "test_heatmap_mode_shows_only_period_controls PASS" << std::endl;
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    ThemeManager::instance()->setTheme(ThemeManager::Light);
    test_trend_card_keeps_header_controls_separate();
    test_heatmap_uses_available_card_height();
    test_heatmap_period_switch_keeps_outer_geometry_stable();
    test_heatmap_mode_shows_only_period_controls();
    std::cout << "All trend card geometry tests passed!" << std::endl;
    return 0;
}
