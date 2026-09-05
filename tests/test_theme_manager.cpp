#include <QApplication>
#include <QTemporaryDir>
#include <QtGlobal>

#include <cassert>

#include "database/database_manager.h"
#include "ui/design_tokens.h"
#include "ui/theme_manager.h"

namespace {

qreal lightnessOf(const QColor &color)
{
    float h = 0, s = 0, l = 0, a = 0;
    color.getHslF(&h, &s, &l, &a);
    return l;
}

} // namespace

int main(int argc, char *argv[])
{
    // ThemeManager::applyToApplication 操作应用调色板与样式表，
    // 需要 QApplication 实例（纯 QCoreApplication 会段错误）。
    QApplication app(argc, argv);

    QTemporaryDir dir;
    assert(dir.isValid());
    DatabaseManager db(dir.filePath(QStringLiteral("data.db")));

    ThemeManager *tm = ThemeManager::instance();
    tm->loadFromDb(&db);

    // 默认状态：未自定义主题色，走内置玉色 token。
    assert(!tm->hasCustomAccent());
    assert(!tm->accentColor().isValid());
    const QColor defaultAccent = ThemeManager::instance()->isDark()
        ? QColor(QStringLiteral("#3DCFB0"))
        : QColor(QStringLiteral("#0B7A66"));
    assert(DesignTokens::kAccent() == defaultAccent);

    // 自定义主题色立即生效并持久化。
    const QColor rose(QStringLiteral("#C2185B"));
    tm->setAccentColor(rose, true);
    assert(tm->hasCustomAccent());
    assert(tm->accentColor() == rose);
    assert(QColor(db.getSetting(QStringLiteral("accent_color"))).name()
           == rose.name());

    // 派生档位：亮色档压暗、暗色档提亮，与当前主题无关。
    // QColor 的 HSL↔RGB 往返转换存在 float 精度误差，容差取 1e-3。
    assert(lightnessOf(DesignTokens::accentFor(false)) <= 0.45 + 1e-3);
    assert(lightnessOf(DesignTokens::accentFor(true)) >= 0.55 - 1e-3);

    // persist=false 只改内存预览，不落库。
    const QColor sea(QStringLiteral("#1565C0"));
    tm->setAccentColor(sea, false);
    assert(tm->accentColor() == sea);
    assert(QColor(db.getSetting(QStringLiteral("accent_color"))).name()
           == rose.name());

    // 模拟重启：以 persist=false 清掉内存自定义态（不落库），再从数据库
    // 加载，应恢复此前落库的自定义主题色。
    tm->setAccentColor(QColor(), false);
    assert(!tm->hasCustomAccent());
    tm->loadFromDb(&db);
    assert(tm->hasCustomAccent());
    assert(tm->accentColor() == rose);

    // 恢复默认落库哨兵值，重启后回到内置玉色路径。
    tm->setAccentColor(QColor(), true);
    assert(!tm->hasCustomAccent());
    assert(db.getSetting(QStringLiteral("accent_color"),
                         QStringLiteral("x")) == QStringLiteral("default"));
    tm->loadFromDb(&db);
    assert(!tm->hasCustomAccent());
    assert(DesignTokens::kAccent() == defaultAccent);

    return 0;
}
