#pragma once

// 组别图标约定：以 emoji 文本形式存放在 app_groups.icon 列，无二进制资源。
// 预设组别有固定图标（presetIconFor）；自定义组别在编辑对话框中挑选，
// 空值时按名称哈希从色板取回退图标（fallbackIcon），保证每个组别恒有图标。
// 本头文件只依赖 Qt Core，数据库层与 UI 层共用同一份定义。

#include <QString>
#include <QStringList>

namespace GroupIcons {

/// 新建/编辑组别时可挑选的图标色板（预设图标也包含在内）。
inline const QStringList &palette()
{
    static const QStringList kPalette = {
        QString::fromUtf8("💻"), QString::fromUtf8("🎨"), QString::fromUtf8("💬"),
        QString::fromUtf8("🎬"), QString::fromUtf8("📄"), QString::fromUtf8("🎮"),
        QString::fromUtf8("📚"), QString::fromUtf8("🌐"), QString::fromUtf8("🎵"),
        QString::fromUtf8("📷"), QString::fromUtf8("⚙"), QString::fromUtf8("🧪"),
        QString::fromUtf8("📊"), QString::fromUtf8("✉"), QString::fromUtf8("🛒"),
        QString::fromUtf8("🗂"), QString::fromUtf8("🎯"), QString::fromUtf8("🏃"),
        QString::fromUtf8("🍔"), QString::fromUtf8("🧠"), QString::fromUtf8("📱"),
        QString::fromUtf8("🖥"), QString::fromUtf8("🎧"), QString::fromUtf8("✍"),
        QString::fromUtf8("💡"), QString::fromUtf8("🗓"), QString::fromUtf8("✅"),
        QString::fromUtf8("🔒"), QString::fromUtf8("☁"), QString::fromUtf8("🧩"),
    };
    return kPalette;
}

/// 预设组别的固定图标；非预设名返回空串。
inline QString presetIconFor(const QString &name)
{
    static const struct {
        const char *name;
        const char *icon;
    } kPresets[] = {
        { "\xe5\xbc\x80\xe5\x8f\x91\xe6\x95\x88\xe7\x8e\x87", "\xf0\x9f\x92\xbb" }, // 开发效率 → 💻
        { "\xe8\xae\xbe\xe8\xae\xa1\xe5\x88\x9b\xe4\xbd\x9c", "\xf0\x9f\x8e\xa8" }, // 设计创作 → 🎨
        { "\xe7\xa4\xbe\xe4\xba\xa4\xe9\x80\x9a\xe8\xae\xaf", "\xf0\x9f\x92\xac" }, // 社交通讯 → 💬
        { "\xe5\xbd\xb1\xe9\x9f\xb3\xe5\xa8\xb1\xe4\xb9\x90", "\xf0\x9f\x8e\xac" }, // 影音娱乐 → 🎬
        { "\xe5\x8a\x9e\xe5\x85\xac\xe6\x96\x87\xe6\xa1\xa3", "\xf0\x9f\x93\x84" }, // 办公文档 → 📄
        { "\xe6\xb8\xb8\xe6\x88\x8f",                          "\xf0\x9f\x8e\xae" }, // 游戏     → 🎮
    };
    for (const auto &preset : kPresets) {
        if (name == QString::fromUtf8(preset.name))
            return QString::fromUtf8(preset.icon);
    }
    return QString();
}

/// 名称哈希回退图标：自行累加码点取模，避免 qHash 种子随进程变化导致
/// 回退图标跨启动不稳定（数据库迁移回填也依赖确定性）。
inline QString fallbackIcon(const QString &name)
{
    const QStringList &colors = palette();
    quint32 sum = 7; // 起始常数让空名/常见短名不至于总落在同一格
    for (const QChar c : name)
        sum = sum * 31u + c.unicode();
    return colors.at(int(sum % quint32(colors.size())));
}

/// 组别应显示的图标：优先自身值，空值时按名称推导。
inline QString displayIcon(const QString &storedIcon, const QString &name)
{
    const QString trimmed = storedIcon.trimmed();
    if (!trimmed.isEmpty())
        return trimmed;
    const QString preset = presetIconFor(name);
    return preset.isEmpty() ? fallbackIcon(name) : preset;
}

} // namespace GroupIcons
