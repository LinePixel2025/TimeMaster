#ifndef THEME_MANAGER_H
#define THEME_MANAGER_H

#include <QColor>
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

    /// 用户自定义的主题色（brand 基准色）；未自定义时返回无效 QColor。
    QColor accentColor() const;
    bool hasCustomAccent() const;
    /// 内置默认主题色（玉色亮色档），供设置界面展示与比较。
    static QColor defaultAccent();
    /// 切换主题色；color 无效表示恢复默认。persist=false 仅即时预览、不落库。
    void setAccentColor(const QColor &color, bool persist = true);
    /// 预览结束后确认落库：把当前内存主题色写入设置表。
    void commitAccent();

signals:
    void themeChanged(Theme theme);
    void accentChanged();

private:
    explicit ThemeManager(QObject *parent = nullptr);
    void applyToApplication();
    void saveToDb(Theme theme);
    void saveAccentToDb();

    Theme m_theme = Light;
    QColor m_accent; // invalid = 使用内置默认玉色
    DatabaseManager *m_db = nullptr;
};

#endif // THEME_MANAGER_H
