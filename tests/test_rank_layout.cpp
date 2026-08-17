#include <cassert>
#include <cmath>
#include <iostream>

#include <QCoreApplication>

#include "ui/rank_layout.h"
#include "ui/ui_utils.h"

namespace {

QVariantMap makeItem(const QString &name, const QString &process, int seconds)
{
    return {
        {QStringLiteral("app_name"), name},
        {QStringLiteral("process_name"), process},
        {QStringLiteral("total_seconds"), seconds},
    };
}

void test_normalize_filters_sorts_and_reindexes()
{
    const QVector<QVariantMap> input = {
        makeItem(QStringLiteral("Zero"), QStringLiteral("zero.exe"), 0),
        makeItem(QStringLiteral("Edge"), QStringLiteral("msedge.exe"), 120),
        makeItem(QStringLiteral("Code"), QStringLiteral("code.exe"), 300),
        makeItem(QStringLiteral("Negative"), QStringLiteral("negative.exe"), -8),
        makeItem(QStringLiteral("ZCode"), QStringLiteral("zcode.exe"), 180),
    };

    const QVector<RankLayout::Item> items = RankLayout::normalize(input);
    assert(items.size() == 3);
    assert(items[0].appName == QStringLiteral("Code"));
    assert(items[1].appName == QStringLiteral("ZCode"));
    assert(items[2].appName == QStringLiteral("Edge"));
    assert(items[0].rank == 1);
    assert(items[1].rank == 2);
    assert(items[2].rank == 3);
    std::cout << "test_normalize_filters_sorts_and_reindexes PASS" << std::endl;
}

void test_normalize_uses_total_usage_share()
{
    const QVector<RankLayout::Item> items = RankLayout::normalize({
        makeItem(QStringLiteral("A"), QStringLiteral("a.exe"), 180),
        makeItem(QStringLiteral("B"), QStringLiteral("b.exe"), 60),
        makeItem(QStringLiteral("C"), QStringLiteral("c.exe"), 60),
    });

    assert(items.size() == 3);
    assert(std::abs(items[0].share - 0.6) < 0.0001);
    assert(std::abs(items[1].share - 0.2) < 0.0001);
    assert(items[0].sharePercent == 60);
    assert(items[1].sharePercent == 20);
    assert(items[2].sharePercent == 20);
    std::cout << "test_normalize_uses_total_usage_share PASS" << std::endl;
}

void test_rank_duration_keeps_short_usage_visible()
{
    assert(UiUtils::formatRankDuration(0) == QStringLiteral("0s"));
    assert(UiUtils::formatRankDuration(42) == QStringLiteral("42s"));
    assert(UiUtils::formatRankDuration(60) == QStringLiteral("1m"));
    assert(UiUtils::formatRankDuration(3660) == QStringLiteral("1h 1m"));
    std::cout << "test_rank_duration_keeps_short_usage_visible PASS" << std::endl;
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    test_normalize_filters_sorts_and_reindexes();
    test_normalize_uses_total_usage_share();
    test_rank_duration_keeps_short_usage_visible();
    std::cout << "All rank layout tests passed!" << std::endl;
    return 0;
}
