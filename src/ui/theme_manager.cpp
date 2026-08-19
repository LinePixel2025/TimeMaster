#include "ui/theme_manager.h"
#include "database/database_manager.h"
#include "ui/design_tokens.h"

#include <QApplication>
#include <QPalette>
#include <QRegularExpression>

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
    applyToApplication();
}

void ThemeManager::saveToDb(Theme theme)
{
    if (m_db) {
        m_db->setSetting("theme", theme == Dark ? "dark" : "light");
    }
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
