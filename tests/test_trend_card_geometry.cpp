#include <cassert>
#include <iostream>

#include <QApplication>
#include <QPushButton>
#include <QWidget>

#include "ui/theme_manager.h"
#include "ui/trend_card.h"
#include "ui/trend_chart_layout.h"

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
    assert(format->height() > 0 && bar->height() > 0 && line->height() > 0);
    std::cout << "test_trend_card_keeps_header_controls_separate PASS" << std::endl;
}

void test_month_heatmap_height_tracks_card_width()
{
    QWidget host;
    host.resize(1000, 700);
    auto *card = new TrendCard(&host);
    card->setDisplayFormat(QStringLiteral("heatmap"));
    card->setHeatmapPeriod(QStringLiteral("month"));
    card->setGeometry(0, 0, 420, 520);
    card->show();
    host.show();
    processEvents();

    auto *chart = card->findChild<QWidget *>(QStringLiteral("trendChartArea"));
    assert(chart);
    const int narrowHeight = chart->height();
    const int expectedNarrow = TrendChartLayout::preferredMonthHeatmapHeight(
        chart->width(), QDate::currentDate());
    assert(narrowHeight == expectedNarrow);

    card->setGeometry(0, 0, 760, 520);
    processEvents();
    const int wideHeight = chart->height();
    const int expectedWide = TrendChartLayout::preferredMonthHeatmapHeight(
        chart->width(), QDate::currentDate());
    assert(wideHeight == expectedWide);
    assert(wideHeight >= narrowHeight);

    const auto layout = TrendChartLayout::makeMonthHeatmapLayout(chart->size(), QDate::currentDate());
    assert(layout.cellWidth == layout.cellHeight);
    assert(card->minimumHeight() >= chart->height());
    std::cout << "test_month_heatmap_height_tracks_card_width PASS" << std::endl;
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
    test_month_heatmap_height_tracks_card_width();
    test_heatmap_mode_shows_only_period_controls();
    std::cout << "All trend card geometry tests passed!" << std::endl;
    return 0;
}
