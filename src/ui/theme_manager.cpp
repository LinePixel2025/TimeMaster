#include "ui/theme_manager.h"
#include "database/database_manager.h"

#include <QApplication>
#include <QPalette>

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
    setTheme(saved == "dark" ? Dark : Light);
}

void ThemeManager::saveToDb(Theme theme)
{
    if (m_db) {
        m_db->setSetting("theme", theme == Dark ? "dark" : "light");
    }
}

void ThemeManager::applyToApplication()
{
    QPalette pal = qApp->palette();

    if (m_theme == Dark) {
        pal.setColor(QPalette::Window, QColor("#1E1E2E"));
        pal.setColor(QPalette::Base, QColor("#2D2D3F"));
        pal.setColor(QPalette::AlternateBase, QColor("#252538"));
        pal.setColor(QPalette::Text, QColor("#F1F5F9"));
        pal.setColor(QPalette::WindowText, QColor("#F1F5F9"));
        pal.setColor(QPalette::Button, QColor("#2D2D3F"));
        pal.setColor(QPalette::ButtonText, QColor("#F1F5F9"));
        pal.setColor(QPalette::Highlight, QColor("#818CF8"));
        pal.setColor(QPalette::HighlightedText, QColor("#1E1E2E"));
        pal.setColor(QPalette::ToolTipBase, QColor("#2D2D3F"));
        pal.setColor(QPalette::ToolTipText, QColor("#F1F5F9"));
    } else {
        pal.setColor(QPalette::Window, QColor("#F0F2F5"));
        pal.setColor(QPalette::Base, QColor("#FFFFFF"));
        pal.setColor(QPalette::AlternateBase, QColor("#F8F9FB"));
        pal.setColor(QPalette::Text, QColor("#1F2937"));
        pal.setColor(QPalette::WindowText, QColor("#1F2937"));
        pal.setColor(QPalette::Button, QColor("#FFFFFF"));
        pal.setColor(QPalette::ButtonText, QColor("#1F2937"));
        pal.setColor(QPalette::Highlight, QColor("#6366F1"));
        pal.setColor(QPalette::HighlightedText, QColor("#FFFFFF"));
        pal.setColor(QPalette::ToolTipBase, QColor("#FFFFFF"));
        pal.setColor(QPalette::ToolTipText, QColor("#1F2937"));
    }

    qApp->setPalette(pal);
}
