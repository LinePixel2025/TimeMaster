#ifndef THEME_MANAGER_H
#define THEME_MANAGER_H

#include <QObject>

class DatabaseManager;

class ThemeManager : public QObject
{
    Q_OBJECT
public:
    enum Theme { Light, Dark };

    static ThemeManager *instance();

    Theme currentTheme() const;
    bool isDark() const;
    void setTheme(Theme theme);
    void toggle();
    void loadFromDb(DatabaseManager *db);

signals:
    void themeChanged(Theme theme);

private:
    explicit ThemeManager(QObject *parent = nullptr);
    void applyToApplication();
    void saveToDb(Theme theme);

    Theme m_theme = Light;
    DatabaseManager *m_db = nullptr;
};

#endif // THEME_MANAGER_H
