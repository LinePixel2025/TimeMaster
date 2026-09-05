#include "update_dialog.h"
#include "ui/theme_manager.h"
#include "ui/design_tokens.h"

#include <QApplication>
#include <QDesktopServices>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QRegularExpression>
#include <QTextBrowser>
#include <QUrl>
#include <QVBoxLayout>

namespace {

/// Release Notes（markdown）转为可供 QTextBrowser 渲染的轻量 HTML：
/// 支持 #/##/### 标题、**加粗**、- 列表、自动链接、< > & 转义。
QString markdownToHtml(const QString &md)
{
    QRegularExpression boldRe(QStringLiteral(R"(\*\*(.+?)\*\*)"));
    QRegularExpression urlRe(
        QStringLiteral(R"((https?://[^\s<>"]+))"));

    const auto inlineFormat = [&boldRe, &urlRe](QString text) {
        text.replace(boldRe, QStringLiteral("<b>\\1</b>"));
        text.replace(urlRe,
                     QStringLiteral("<a href=\"\\1\" style=\"color:%1;\">\\1</a>")
                         .arg(DesignTokens::kAccent().name()));
        return text;
    };

    QString html;
    bool inList = false;
    const QStringList lines = md.split(QLatin1Char('\n'));
    for (QString line : lines) {
        line = line.toHtmlEscaped();
        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty()) {
            if (inList) {
                html += QStringLiteral("</ul>");
                inList = false;
            }
            html += QStringLiteral("<br/>");
            continue;
        }
        if (trimmed.startsWith(QStringLiteral("### "))) {
            if (inList) { html += QLatin1String("</ul>"); inList = false; }
            html += QStringLiteral("<h4>") + inlineFormat(trimmed.mid(4))
                    + QStringLiteral("</h4>");
            continue;
        }
        if (trimmed.startsWith(QStringLiteral("## "))) {
            if (inList) { html += QLatin1String("</ul>"); inList = false; }
            html += QStringLiteral("<h3>") + inlineFormat(trimmed.mid(3))
                    + QStringLiteral("</h3>");
            continue;
        }
        if (trimmed.startsWith(QStringLiteral("# "))) {
            if (inList) { html += QLatin1String("</ul>"); inList = false; }
            html += QStringLiteral("<h2>") + inlineFormat(trimmed.mid(2))
                    + QStringLiteral("</h2>");
            continue;
        }
        if (trimmed.startsWith(QStringLiteral("- "))) {
            if (!inList) {
                html += QStringLiteral("<ul>");
                inList = true;
            }
            html += QStringLiteral("<li>") + inlineFormat(trimmed.mid(2))
                    + QStringLiteral("</li>");
            continue;
        }
        if (trimmed.startsWith(QStringLiteral("&gt; "))) { // "> " 转义后
            if (inList) { html += QLatin1String("</ul>"); inList = false; }
            html += QStringLiteral("<p style=\"color:%1;\">")
                        .arg(DesignTokens::kTextMute().name())
                    + inlineFormat(trimmed.mid(5))
                    + QStringLiteral("</p>");
            continue;
        }
        if (inList) {
            html += QStringLiteral("</ul>");
            inList = false;
        }
        html += QStringLiteral("<p>") + inlineFormat(trimmed)
                + QStringLiteral("</p>");
    }
    if (inList)
        html += QStringLiteral("</ul>");
    return html;
}

/// 弹窗样式：颜色全套取 DesignTokens，与设置页按钮观感一致。
QString updateDialogStyle()
{
    const QString bg          = DesignTokens::kBg().name(QColor::HexArgb);
    const QString surface     = DesignTokens::kSurface().name(QColor::HexArgb);
    const QString border      = DesignTokens::kBorder().name(QColor::HexArgb);
    const QString text        = DesignTokens::kText().name(QColor::HexArgb);
    const QString textStrong  = DesignTokens::kTextStrong().name(QColor::HexArgb);
    const QString textMute    = DesignTokens::kTextMute().name(QColor::HexArgb);
    const QString textFaint   = DesignTokens::kTextFaint().name(QColor::HexArgb);
    const QString accent      = DesignTokens::kAccent().name(QColor::HexArgb);
    const QString onAccent    = DesignTokens::kOnAccent().name(QColor::HexArgb);
    const QString focus       = DesignTokens::kFocusBorder().name(QColor::HexArgb);
    const QString accentHover = DesignTokens::kAccentHover().name(QColor::HexArgb);
    const QString accentPress = DesignTokens::kAccentPressed().name(QColor::HexArgb);
    const QString accentLight = DesignTokens::kAccentLight().name(QColor::HexArgb);
    const QString hoverBg     = DesignTokens::kButtonHoverBg().name(QColor::HexArgb);

    return QStringLiteral(
        "QDialog { background: %1; }"
        "QLabel#updateTitle { color: %5; background: transparent; }"
        "QLabel#updateSubtitle { color: %7; background: transparent; }"
        "QLabel#updateHint { color: %8; background: transparent; }"
        "QTextBrowser#updateNotes { color: %4; background: %2;"
        " border: 1px solid %3; border-radius: 8px; padding: 10px;"
        " selection-background-color: %12; }"
        "QTextBrowser#updateNotes h2, QTextBrowser#updateNotes h3,"
        " QTextBrowser#updateNotes h4 { color: %5; }"

        "QPushButton#accentBtn { background: %6; color: %15;"
        " border: 1px solid transparent; border-radius: 6px;"
        " padding: 8px 22px; font-size: 13px; font-weight: 600; }"
        "QPushButton#accentBtn:hover { background: %10; }"
        "QPushButton#accentBtn:pressed { background: %11; }"
        "QPushButton#accentBtn:focus { border-color: %14; }"
        "QPushButton#accentBtn:disabled { background: %3; color: %8; }"

        "QPushButton#secondaryBtn { background: %2; color: %4;"
        " border: 1px solid %3; border-radius: 6px; padding: 7px 14px;"
        " font-size: 13px; }"
        "QPushButton#secondaryBtn:hover { background: %13; }"
        "QPushButton#secondaryBtn:pressed { background: %12; }"
        "QPushButton#secondaryBtn:focus { border-color: %14; }"

        "QScrollBar:vertical { background: transparent; width: 10px; margin: 2px; }"
        "QScrollBar::handle:vertical { background: %8; border-radius: 5px;"
        " min-height: 24px; }"
        "QScrollBar::handle:vertical:hover { background: %7; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical"
        " { background: transparent; }")
        .arg(bg)
        .arg(surface)
        .arg(border)
        .arg(text)
        .arg(textStrong)
        .arg(accent)
        .arg(textMute)
        .arg(textFaint)
        .arg(accentHover)
        .arg(accentPress)
        .arg(accentLight)
        .arg(hoverBg)
        .arg(focus)
        .arg(onAccent);
}

} // namespace

UpdateDialog::UpdateDialog(const UpdateInfo &info, QWidget *parent)
    : QDialog(parent), m_info(info)
{
    setWindowTitle(QString::fromUtf8("发现新版本"));
    setModal(true);
    resize(560, 480);
    setMinimumSize(480, 380);

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(24, 20, 24, 18);
    rootLayout->setSpacing(12);

    // 标题行：图标 + 版本信息。
    auto *headerRow = new QHBoxLayout();
    headerRow->setSpacing(14);
    auto *iconLabel = new QLabel(this);
    iconLabel->setPixmap(QIcon(QStringLiteral(":/icon.svg")).pixmap(48, 48));
    headerRow->addWidget(iconLabel, 0, Qt::AlignTop);

    auto *titleColumn = new QVBoxLayout();
    titleColumn->setSpacing(4);
    m_titleLabel = new QLabel(
        QString::fromUtf8("发现新版本 v%1").arg(m_info.version), this);
    m_titleLabel->setObjectName(QStringLiteral("updateTitle"));
    m_titleLabel->setFont(DesignTokens::appFont(19, QFont::Bold));
    QString subtitle =
        QString::fromUtf8("当前版本 v%1 → v%2")
            .arg(QApplication::applicationVersion(), m_info.version);
    if (!m_info.publishedAt.isEmpty())
        subtitle += QString::fromUtf8(" · 发布于 %1").arg(m_info.publishedAt);
    m_subtitleLabel = new QLabel(subtitle, this);
    m_subtitleLabel->setObjectName(QStringLiteral("updateSubtitle"));
    m_subtitleLabel->setFont(DesignTokens::appFont(12));
    titleColumn->addWidget(m_titleLabel);
    titleColumn->addWidget(m_subtitleLabel);
    headerRow->addLayout(titleColumn, 1);
    rootLayout->addLayout(headerRow);

    // 更新内容区。
    m_notesView = new QTextBrowser(this);
    m_notesView->setObjectName(QStringLiteral("updateNotes"));
    m_notesView->setFont(DesignTokens::appFont(12));
    m_notesView->setOpenExternalLinks(true);
    m_notesView->setMinimumHeight(180);
    m_notesView->setMaximumHeight(340);
    m_notesView->setHtml(markdownToHtml(m_info.notes));
    rootLayout->addWidget(m_notesView, 1);

    // 安装说明小字。
    auto *hintLabel = new QLabel(
        QString::fromUtf8("「立即更新」将在浏览器打开安装包下载页，"
                          "下载完成后运行安装程序即可完成升级，已有数据不会丢失。"), this);
    hintLabel->setObjectName(QStringLiteral("updateHint"));
    hintLabel->setFont(DesignTokens::appFont(11));
    hintLabel->setWordWrap(true);
    rootLayout->addWidget(hintLabel);

    // 按钮行。
    auto *btnRow = new QHBoxLayout();
    btnRow->setSpacing(10);
    auto *laterBtn = new QPushButton(QString::fromUtf8("稍后"), this);
    laterBtn->setObjectName(QStringLiteral("secondaryBtn"));
    laterBtn->setToolTip(QString::fromUtf8("暂不更新，稍后可在设置 → 关于中再次检查"));
    connect(laterBtn, &QPushButton::clicked, this, &QDialog::reject);
    m_updateBtn = new QPushButton(QString::fromUtf8("立即更新"), this);
    m_updateBtn->setObjectName(QStringLiteral("accentBtn"));
    m_updateBtn->setToolTip(QString::fromUtf8("在浏览器打开安装包下载页"));
    if (m_info.downloadUrl.isEmpty() && m_info.releaseUrl.isEmpty())
        m_updateBtn->setEnabled(false);
    connect(m_updateBtn, &QPushButton::clicked,
            this, &UpdateDialog::openDownload);
    btnRow->addStretch();
    btnRow->addWidget(laterBtn);
    btnRow->addWidget(m_updateBtn);
    rootLayout->addLayout(btnRow);

    applyTheme();
    connect(ThemeManager::instance(), &ThemeManager::themeChanged,
            this, [this](ThemeManager::Theme) {
        applyTheme();
        if (isVisible())
            ThemeManager::applyToWindow(this);
    });
    connect(ThemeManager::instance(), &ThemeManager::accentChanged,
            this, [this]() { applyTheme(); });
}

void UpdateDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    ThemeManager::applyToWindow(this);
}

void UpdateDialog::applyTheme()
{
    QPalette pal = palette();
    pal.setColor(QPalette::Window, DesignTokens::kBg());
    pal.setColor(QPalette::Base, DesignTokens::kSurface());
    pal.setColor(QPalette::Text, DesignTokens::kTextStrong());
    pal.setColor(QPalette::WindowText, DesignTokens::kTextStrong());
    setPalette(pal);
    setStyleSheet(updateDialogStyle());
}

void UpdateDialog::openDownload()
{
    const QString url = m_info.downloadUrl.isEmpty()
                            ? m_info.releaseUrl
                            : m_info.downloadUrl;
    if (!url.isEmpty())
        QDesktopServices::openUrl(QUrl(url));
    accept();
}
