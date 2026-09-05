#include "ui/theme_manager.h"
#include "database/database_manager.h"
#include "ui/design_tokens.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dwmapi.h>

#include <QApplication>
#include <QPalette>
#include <QRegularExpression>
#include <QWidget>

// MinGW 的 dwmapi.h 可能未提供这两个较新的 DWM 属性编号，与 main_window.cpp 原实现一致。
static const DWORD kDwmwaUseImmersiveDarkMode = 20;
static const DWORD kDwmwaCaptionColor = 35;

ThemeManager *ThemeManager::instance()
{
    static ThemeManager inst;
    return &inst;
}

ThemeManager::ThemeManager(QObject *parent)
    : QObject(parent)
{
}

ThemeManager::Theme ThemeManager::currentTheme() const
{
    return m_theme;
}

bool ThemeManager::isDark() const
{
    return m_theme == Dark;
}

void ThemeManager::toggle()
{
    setTheme(m_theme == Dark ? Light : Dark);
}

void ThemeManager::setTheme(Theme theme)
{
    if (m_theme == theme)
        return;
    m_theme = theme;
    applyToApplication();
    saveToDb(theme);
    emit themeChanged(theme);
}

void ThemeManager::loadFromDb(DatabaseManager *db)
{
    m_db = db;
    QString saved = db->getSetting("theme", "light");
    m_theme = (saved == "dark") ? Dark : Light;
    // settings 表 value 列有 NOT NULL 约束，恢复默认时以哨兵值 "default" 落库。
    const QString accent = db->getSetting("accent_color");
    m_accent = (accent.isEmpty() || accent == QStringLiteral("default"))
        ? QColor() : QColor(accent);
    applyToApplication();
}

void ThemeManager::saveToDb(Theme theme)
{
    if (m_db) {
        m_db->setSetting("theme", theme == Dark ? "dark" : "light");
    }
}

QColor ThemeManager::defaultAccent()
{
    return QColor("#0B7A66");
}

QColor ThemeManager::accentColor() const
{
    return m_accent;
}

bool ThemeManager::hasCustomAccent() const
{
    return m_accent.isValid();
}

void ThemeManager::setAccentColor(const QColor &color, bool persist)
{
    // name() 规范化为 #rrggbb，主题色不携带 alpha。
    const QColor next = color.isValid() ? QColor(color.name()) : QColor();
    if (next.isValid() == m_accent.isValid()
        && (!next.isValid() || next == m_accent))
        return;
    m_accent = next;
    applyToApplication();
    if (persist)
        saveAccentToDb();
    emit accentChanged();
}

void ThemeManager::commitAccent()
{
    saveAccentToDb();
}

void ThemeManager::saveAccentToDb()
{
    if (m_db) {
        m_db->setSetting("accent_color",
                         m_accent.isValid() ? m_accent.name()
                                            : QStringLiteral("default"));
    }
}

void ThemeManager::applyToWindow(QWidget *window)
{
    if (!window)
        return;
    // QPalette 只影响客户区，Windows 标题栏由 DWM 决定，需单独设置，
    // 否则对话框标题栏回落系统默认色，与主题化后的内容区割裂。
    HWND hwnd = reinterpret_cast<HWND>(window->winId());
    const BOOL dark = instance()->isDark() ? TRUE : FALSE;
    DwmSetWindowAttribute(hwnd, kDwmwaUseImmersiveDarkMode, &dark, sizeof(dark));
    const QColor bg = DesignTokens::kBg();
    const COLORREF color = RGB(bg.red(), bg.green(), bg.blue());
    DwmSetWindowAttribute(hwnd, kDwmwaCaptionColor, &color, sizeof(color));
}

void ThemeManager::applyToApplication()
{
    // 必须先写入 m_theme 再取 token：DesignTokens 通过 isDark() 读当前主题。
    const QColor bg = DesignTokens::kBg();
    const QColor surface = DesignTokens::kSurface();
    const QColor text = DesignTokens::kTextStrong();
    const QColor accent = DesignTokens::kAccent();
    const QColor hover = DesignTokens::kButtonHoverBg();

    QPalette pal = qApp->palette();
    pal.setColor(QPalette::Window, bg);
    pal.setColor(QPalette::Base, surface);
    pal.setColor(QPalette::AlternateBase, hover);
    pal.setColor(QPalette::Text, text);
    pal.setColor(QPalette::WindowText, text);
    pal.setColor(QPalette::Button, surface);
    pal.setColor(QPalette::ButtonText, text);
    pal.setColor(QPalette::Highlight, accent);
    pal.setColor(QPalette::HighlightedText, m_theme == Dark ? bg : QColor("#FFFFFF"));
    pal.setColor(QPalette::ToolTipBase, surface);
    pal.setColor(QPalette::ToolTipText, DesignTokens::kText());
    qApp->setPalette(pal);

    QString styleSheet = qApp->styleSheet();
    // 连同块前换行一起移除，避免重复应用主题时垃圾累积导致 QToolTip 规则失效
    //（规则失效时提示气泡会回退系统原生配色，深色系统下出现黑底暗字）。
    static const QRegularExpression tooltipRule(
        QStringLiteral("(?:\\n)?/\\* TimeMasterTooltip begin \\*/.*?/\\* TimeMasterTooltip end \\*/"),
        QRegularExpression::DotMatchesEverythingOption);
    styleSheet.remove(tooltipRule);
    styleSheet += QStringLiteral(
        "\n/* TimeMasterTooltip begin */"
        "QToolTip { background-color: %1; color: %2; border: 1px solid %3;"
        " padding: 6px 8px; font-size: 12px; }"
        "/* TimeMasterTooltip end */")
        .arg(DesignTokens::kSurface().name(),
             DesignTokens::kText().name(),
             DesignTokens::kBorder().name());
    qApp->setStyleSheet(styleSheet);
}
