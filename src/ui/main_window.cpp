#include "ui/main_window.h"
#include "database/database_manager.h"
#include "ai/ai_client.h"
#include "ui/settings_dialog.h"
#include "ui/settings_icons.h"
#include "ui/theme_manager.h"
#include "ui/design_tokens.h"
#include "ui/dashboard_layout.h"
#include "ui/ui_utils.h"
#include "ui/hero_card.h"
#include "ui/trend_card.h"
#include "ui/rank_card.h"
#include "ui/ai_report_card.h"
#include "export/exporter.h"
#include "report/session_hours.h"

#include <QCloseEvent>
#include <QShowEvent>
#include <QResizeEvent>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLayout>
#include <QGridLayout>
#include <QPushButton>
#include <QInputDialog>
#include <QFileDialog>
#include <QMessageBox>
#include <QMenu>
#include <QWidget>
#include <QApplication>
#include <QLabel>
#include <QScreen>
#include <QScrollArea>
#include <QGuiApplication>
#include <QDate>
#include <QDateTime>

#include <array>

#include <windows.h>
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")

static const DWORD DWMWA_USE_IMMERSIVE_DARK_MODE = 20;
static const DWORD DWMWA_CAPTION_COLOR = 35;

static void applyDwmTitleBar(HWND hwnd, bool dark)
{
    BOOL dwmDark = dark ? TRUE : FALSE;
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dwmDark, sizeof(dwmDark));
    QColor bg = DesignTokens::kBg();
    COLORREF color = RGB(bg.red(), bg.green(), bg.blue());
    DwmSetWindowAttribute(hwnd, DWMWA_CAPTION_COLOR, &color, sizeof(color));
}

MainWindow::MainWindow(DatabaseManager *db, AiClient *ai, QWidget *parent)
    : QMainWindow(parent), m_db(db)
{
    setWindowTitle("Time Master");
    setMinimumSize(840, 640);

    // 默认以当前允许的最小尺寸启动（最紧凑的完整布局），用户可再手动放大。
    resize(minimumSize());

    QColor bg = DesignTokens::kBg();
    QColor textStrong = DesignTokens::kTextStrong();

    QPalette pal = palette();
    pal.setColor(QPalette::Window, bg);
    pal.setColor(QPalette::Base, DesignTokens::kSurface());
    pal.setColor(QPalette::Text, textStrong);
    pal.setColor(QPalette::WindowText, textStrong);
    setPalette(pal);

    auto *central = new QWidget(this);
    central->setObjectName("centralWidget");
    central->setStyleSheet(QString("#centralWidget { background-color: %1; }").arg(bg.name()));
    setCentralWidget(central);

    m_rootLayout = new QVBoxLayout(central);
    m_rootLayout->setContentsMargins(DesignTokens::kOuterMargin,
                                     DesignTokens::kWindowTopMargin,
                                     DesignTokens::kOuterMargin,
                                     DesignTokens::kWindowBottomMargin);
    m_rootLayout->setSpacing(DesignTokens::kSectionSpacing);

    auto *headerLayout = new QHBoxLayout();
    headerLayout->setSpacing(DesignTokens::kHeaderSpacing);

    auto *titleColumn = new QVBoxLayout();
    titleColumn->setSpacing(DesignTokens::kTitleStackSpacing);
    m_titleLabel = new QLabel("Time Master", central);
    m_titleLabel->setFont(DesignTokens::appFont(20, QFont::DemiBold));
    titleColumn->addWidget(m_titleLabel);
    m_dateLabel = new QLabel(dateText(), central);
    m_dateLabel->setFont(DesignTokens::appFont(11));
    titleColumn->addWidget(m_dateLabel);
    headerLayout->addLayout(titleColumn);
    headerLayout->addStretch();

    m_statusChip = new QLabel(QStringLiteral("空闲"), central);
    m_statusChip->setObjectName(QStringLiteral("statusChip"));
    m_statusChip->setFont(DesignTokens::appFont(11, QFont::Medium));
    m_statusChip->setAlignment(Qt::AlignVCenter);
    m_statusChip->setMaximumWidth(DesignTokens::kStatusChipMaxWidth);
    m_statusChip->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    headerLayout->addWidget(m_statusChip);

    m_themeBtn = new QPushButton(central);
    m_themeBtn->setFixedSize(DesignTokens::kIconButtonSize, DesignTokens::kIconButtonSize);
    m_themeBtn->setCursor(Qt::PointingHandCursor);
    connect(m_themeBtn, &QPushButton::clicked, this, []() {
        ThemeManager::instance()->toggle();
    });
    headerLayout->addWidget(m_themeBtn);

    m_moreBtn = new QPushButton(central);
    m_moreBtn->setFixedSize(DesignTokens::kIconButtonSize, DesignTokens::kIconButtonSize);
    m_moreBtn->setCursor(Qt::PointingHandCursor);
    m_moreBtn->setToolTip(QStringLiteral("更多"));
    connect(m_moreBtn, &QPushButton::clicked, this, &MainWindow::onMoreMenu);
    headerLayout->addWidget(m_moreBtn);

    m_settingsBtn = new QPushButton(central);
    m_settingsBtn->setFixedSize(DesignTokens::kIconButtonSize, DesignTokens::kIconButtonSize);
    m_settingsBtn->setCursor(Qt::PointingHandCursor);
    m_settingsBtn->setToolTip(QStringLiteral("设置"));
    connect(m_settingsBtn, &QPushButton::clicked, this, &MainWindow::onSettings);
    headerLayout->addWidget(m_settingsBtn);

    m_rootLayout->addLayout(headerLayout);

    m_dashboardScroll = new QScrollArea(central);
    m_dashboardScroll->setObjectName(QStringLiteral("dashboardScroll"));
    // viewport 透明规则必须用 id 选择器：无选择器规则会随 Qt 的
    // _q_stylesheet_parent 级联到 QToolTip，把提示背景染成透明（黑）。
    m_dashboardScroll->viewport()->setObjectName(QStringLiteral("dashboardViewport"));
    m_dashboardScroll->setWidgetResizable(true);
    m_dashboardScroll->setFrameShape(QFrame::NoFrame);
    m_dashboardScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_dashboardScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    m_dashboardContent = new QWidget(m_dashboardScroll);
    m_dashboardContent->setObjectName(QStringLiteral("dashboardContent"));
    m_dashboardContent->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    auto *dashboardLayout = new QVBoxLayout(m_dashboardContent);
    dashboardLayout->setContentsMargins(0, 0, 0, 0);
    dashboardLayout->setSpacing(0);
    dashboardLayout->setSizeConstraint(QLayout::SetMinimumSize);

    m_grid = new QGridLayout();
    m_grid->setSpacing(DesignTokens::kGridSpacing);
    m_grid->setContentsMargins(0, 0, 0, 0);
    m_grid->setAlignment(Qt::AlignTop);
    m_grid->setColumnStretch(0, 1);
    m_grid->setColumnStretch(1, 1);

    m_heroCard = new HeroCard(m_dashboardContent);
    m_trendCard = new TrendCard(m_dashboardContent);
    m_rankCard = new RankCard(m_dashboardContent);
    m_aiCard = new AiReportCard(ai, m_dashboardContent);
    m_heroCard->setObjectName(QStringLiteral("heroCard"));
    m_trendCard->setObjectName(QStringLiteral("trendCard"));
    m_rankCard->setObjectName(QStringLiteral("rankCard"));
    m_aiCard->setObjectName(QStringLiteral("aiReportCard"));

    // 初始排布与 m_narrowLayout 的初始值（单列）保持一致；
    // 宽窗口在首次 resizeEvent 时会切换为双列。
    m_grid->addWidget(m_heroCard, 0, 0, 1, 2);
    m_grid->addWidget(m_trendCard, 1, 0, 1, 2);
    m_grid->addWidget(m_rankCard, 2, 0, 1, 2);
    m_grid->addWidget(m_aiCard, 3, 0, 1, 2);
    m_grid->setColumnStretch(0, 1);
    m_grid->setColumnStretch(1, 0);

    dashboardLayout->addLayout(m_grid, 0);
    dashboardLayout->addStretch(1);
    m_dashboardScroll->setWidget(m_dashboardContent);
    m_rootLayout->addWidget(m_dashboardScroll, 1);

    connect(m_aiCard, &AiReportCard::generateRequested, this, [this]() {
        emit aiReportRequested(AiPeriod::daily());
    });
    connect(m_aiCard, &AiReportCard::dailyReportOpenRequested,
            this, &MainWindow::dailyReportOpenRequested);
    connect(m_aiCard, &AiReportCard::yesterdayReportOpenRequested,
            this, &MainWindow::yesterdayReportOpenRequested);
    connect(m_aiCard, &AiReportCard::weeklyReportOpenRequested,
            this, &MainWindow::weeklyReportOpenRequested);
    connect(m_aiCard, &AiReportCard::weeklyReportGenerateRequested,
            this, &MainWindow::weeklyReportGenerateRequested);
    connect(m_aiCard, &AiReportCard::weeklyReportRegenerateRequested,
            this, &MainWindow::weeklyReportRegenerateRequested);
    connect(this, &MainWindow::settingsChanged, this, [this]() {
        m_aiCard->reloadState();
        updateStatusChip();
    });

    connect(m_trendCard, &TrendCard::chartTypeChanged, this,
            [this](const QString &type) {
        m_db->setSetting("chart_type", type);
    });

    connect(m_trendCard, &TrendCard::heatmapPeriodChanged, this,
            [this](const QString &period) {
        m_db->setSetting("trend_heatmap_period", period);
        refreshData();
    });

    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setInterval(10000);
    connect(m_refreshTimer, &QTimer::timeout, this, &MainWindow::refreshData);
    m_refreshTimer->start();

    connect(ThemeManager::instance(), &ThemeManager::themeChanged,
            this, [this](ThemeManager::Theme theme) {
        applyTheme();
        if (isVisible())
            applyDwmTitleBar(reinterpret_cast<HWND>(winId()),
                             theme == ThemeManager::Dark);
    });
    // 主题色变化只影响 token 颜色，标题栏背景（kBg）不变，无需重设 DWM。
    connect(ThemeManager::instance(), &ThemeManager::accentChanged,
            this, [this]() { applyTheme(); });

    applyTheme();
    updateStatusChip();
    applyResponsiveLayout();
    m_trendCard->setChartType(m_db->getSetting("chart_type", "bar"));
    refreshData();
}

QString MainWindow::dateText() const
{
    static const QString kWeek[] = {
        QStringLiteral("周一"), QStringLiteral("周二"), QStringLiteral("周三"),
        QStringLiteral("周四"), QStringLiteral("周五"), QStringLiteral("周六"),
        QStringLiteral("周日")
    };
    const QDate d = QDate::currentDate();
    return QStringLiteral("%1月%2日 · %3")
        .arg(d.month()).arg(d.day()).arg(kWeek[d.dayOfWeek() - 1]);
}

void MainWindow::applyTheme()
{
    const QColor bg = DesignTokens::kBg();
    QPalette pal = palette();
    pal.setColor(QPalette::Window, bg);
    pal.setColor(QPalette::Base, DesignTokens::kSurface());
    pal.setColor(QPalette::Text, DesignTokens::kTextStrong());
    pal.setColor(QPalette::WindowText, DesignTokens::kTextStrong());
    setPalette(pal);
    centralWidget()->setStyleSheet(
        QString("#centralWidget { background-color: %1; }").arg(bg.name()));
    m_dashboardScroll->setStyleSheet(QStringLiteral(
        "QScrollArea { background: transparent; border: none; }"
        "QScrollBar:vertical { background: transparent; width: 6px; margin: 0; }"
        "QScrollBar::handle:vertical { background: %1; min-height: 24px; border-radius: 3px; }"
        "QScrollBar::handle:vertical:hover { background: %2; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }")
        .arg(DesignTokens::kTextFaint().name(), DesignTokens::kTextMute().name()));
    m_dashboardScroll->viewport()->setStyleSheet(
        QStringLiteral("#dashboardViewport { background: transparent; }"));

    m_titleLabel->setStyleSheet(QString("color: %1; background: transparent;")
        .arg(DesignTokens::kTextStrong().name()));
    m_dateLabel->setStyleSheet(QString("color: %1; background: transparent;")
        .arg(DesignTokens::kTextMute().name()));

    const QString iconStyle = QString(
        "QPushButton { background: transparent; color: %1; border: 1px solid transparent;"
        " border-radius: %2px; font-size: 16px; padding: 0; }"
        "QPushButton:hover { background: %3; border-color: %4; }")
        .arg(DesignTokens::kTextMute().name(),
             QString::number(DesignTokens::kRadiusBtn),
             DesignTokens::kButtonHoverBg().name(),
             DesignTokens::kBorder().name())
        + UiUtils::focusBorderRule();
    m_themeBtn->setStyleSheet(iconStyle);
    m_moreBtn->setStyleSheet(iconStyle);
    m_settingsBtn->setStyleSheet(iconStyle);

    const bool dark = ThemeManager::instance()->isDark();
    m_themeBtn->setIcon(SettingsIcons::icon(
        dark ? SettingsIcons::Sun : SettingsIcons::Moon, 18, DesignTokens::kTextMute()));
    m_themeBtn->setIconSize(QSize(18, 18));
    m_themeBtn->setToolTip(dark ? QStringLiteral("切换到浅色模式")
                                : QStringLiteral("切换到暗色模式"));
    m_moreBtn->setIcon(SettingsIcons::icon(
        SettingsIcons::More, 18, DesignTokens::kTextMute()));
    m_moreBtn->setIconSize(QSize(18, 18));
    m_settingsBtn->setIcon(SettingsIcons::icon(
        SettingsIcons::Gear, 18, DesignTokens::kTextMute()));
    m_settingsBtn->setIconSize(QSize(18, 18));

    updateStatusChip();
}

void MainWindow::updateStatusChip()
{
    m_trackingPaused = (m_db->getSetting("tracking_enabled", "true") != "true");

    QString text;
    QColor fg = DesignTokens::kAccent();
    QColor bg = DesignTokens::kAccentLight();
    if (m_trackingPaused) {
        text = QStringLiteral("已暂停");
        fg = DesignTokens::kTextMute();
        bg = DesignTokens::kButtonHoverBg();
    } else if (m_activeApp.isEmpty()) {
        text = QStringLiteral("空闲");
        fg = DesignTokens::kTextMute();
        bg = DesignTokens::kButtonHoverBg();
    } else {
        text = QStringLiteral("追踪中 · %1").arg(m_activeApp);
    }

    m_statusText = text;
    m_statusChip->setToolTip(m_statusText);
    m_statusChip->setStyleSheet(
        QStringLiteral("QLabel { color: %1; background: %2; border-radius: %3px;"
                       " padding: %4px %5px; }")
            .arg(fg.name(), bg.name(QColor::HexArgb),
                 QString::number(DesignTokens::kRadiusChip),
                 QString::number(DesignTokens::kStatusChipPaddingV),
                 QString::number(DesignTokens::kStatusChipPaddingH)));
    updateStatusChipText();
}

void MainWindow::updateStatusChipText()
{
    if (!m_statusChip)
        return;

    // 按固定最大宽度上限省略：若按当前 contentsRect 省略，芯片会因
    // sizeHint 收缩陷入「越省越窄」的死循环，最终只剩「追…」。
    const int available = DesignTokens::kStatusChipMaxWidth
        - 2 * DesignTokens::kStatusChipPaddingH;
    const QString displayText = QFontMetrics(m_statusChip->font()).elidedText(
        m_statusText, Qt::ElideRight, available);
    m_statusChip->setText(displayText);
}

void MainWindow::applyResponsiveLayout()
{
    int viewportWidth = m_dashboardScroll->viewport()->contentsRect().width();
    if (viewportWidth <= 0)
        viewportWidth = qMax(0, width() - 2 * DesignTokens::kOuterMargin);

    const DashboardLayout::Mode previousMode = m_narrowLayout
        ? DashboardLayout::Mode::SingleColumn
        : DashboardLayout::Mode::DualColumn;
    const bool narrow = DashboardLayout::resolveMode(viewportWidth, previousMode)
        == DashboardLayout::Mode::SingleColumn;
    if (narrow == m_narrowLayout && m_grid->count() == 4) {
        updateStatusChipText();
        return;
    }
    m_narrowLayout = narrow;

    m_grid->removeWidget(m_heroCard);
    m_grid->removeWidget(m_trendCard);
    m_grid->removeWidget(m_rankCard);
    m_grid->removeWidget(m_aiCard);

    if (narrow) {
        m_grid->addWidget(m_heroCard, 0, 0, 1, 2);
        m_grid->addWidget(m_trendCard, 1, 0, 1, 2);
        m_grid->addWidget(m_rankCard, 2, 0, 1, 2);
        m_grid->addWidget(m_aiCard, 3, 0, 1, 2);
        m_grid->setColumnStretch(0, 1);
        m_grid->setColumnStretch(1, 0);
    } else {
        m_grid->addWidget(m_heroCard, 0, 0, 1, 2);
        m_grid->addWidget(m_trendCard, 1, 0);
        m_grid->addWidget(m_rankCard, 1, 1);
        m_grid->addWidget(m_aiCard, 2, 0, 1, 2);
        m_grid->setColumnStretch(0, 3);
        m_grid->setColumnStretch(1, 2);
    }

    for (int row = 0; row < 4; ++row)
        m_grid->setRowStretch(row, 0);
    updateStatusChipText();
}

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    applyDwmTitleBar(reinterpret_cast<HWND>(winId()),
                     ThemeManager::instance()->isDark());
    updateStatusChipText();
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    applyResponsiveLayout();
    updateStatusChipText();
}

void MainWindow::refreshData()
{
    const int todayTotal = m_db->getTodayTotal();
    const int yesterdayTotal = m_db->getYesterdayTotal();
    const int dailyGoal = m_db->getSetting("daily_goal", "28800").toInt();

    std::array<int, 24> hours {};
    const QDate today = QDate::currentDate();
    const auto rows = m_db->getAllSessions(today.toString(Qt::ISODate),
                                           today.toString(Qt::ISODate));
    for (const auto &row : rows) {
        const int secs = row.value(QStringLiteral("duration_seconds")).toInt();
        const QDateTime start = QDateTime::fromString(
            row.value(QStringLiteral("start_time")).toString(), Qt::ISODate);
        SessionHours::addToDayHours(start, secs, today, hours.data());
    }
    m_heroCard->setData(todayTotal, yesterdayTotal, dailyGoal, hours);

    const QString format = m_db->getSetting("trend_display_format", "normal");
    m_trendCard->setDisplayFormat(format);
    m_trendCard->setData(m_db->getWeekSummary());
    if (format == QStringLiteral("heatmap")) {
        m_trendCard->setMonthData(m_db->getMonthSummary());
        m_trendCard->setHeatmapPeriod(
            m_db->getSetting("trend_heatmap_period", "week"));
    }

    m_rankCard->refresh(m_db->getAppRank());
    m_dateLabel->setText(dateText());
    updateStatusChip();
}

void MainWindow::onActiveWindowChanged(const QString &, const QString &,
                                       const QString &appName)
{
    m_activeApp = appName;
    updateStatusChip();
}

void MainWindow::onAiReportReady(const QString &, const QString &text)
{
    m_aiCard->setReport(text);
}

void MainWindow::onAiReportFailed(const QString &, const QString &error)
{
    m_aiCard->showError(error);
}

void MainWindow::onWeeklyReportReady(const QString &path)
{
    m_aiCard->setWeeklyReportPath(path);
}

void MainWindow::onMoreMenu()
{
    QMenu menu(this);
    UiUtils::applyMenuStyle(&menu);
    QAction *exportAct = menu.addAction(QStringLiteral("导出记录"));
    QAction *syncAct = menu.addAction(QStringLiteral("云端同步"));
    QAction *refreshAct = menu.addAction(QStringLiteral("刷新"));
    QAction *chosen = menu.exec(m_moreBtn->mapToGlobal(QPoint(0, m_moreBtn->height())));
    if (chosen == exportAct)
        onExport();
    else if (chosen == syncAct)
        emit cloudSyncRequested();
    else if (chosen == refreshAct)
        refreshData();
}

void MainWindow::onExport()
{
    QStringList formats;
    formats << QStringLiteral("CSV (.csv)")
            << QStringLiteral("Excel (.xlsx)");
    bool ok = false;
    QString fmt = QInputDialog::getItem(this,
        QStringLiteral("导出格式"),
        QStringLiteral("选择导出格式:"),
        formats, 0, false, &ok);
    if (!ok) return;

    QString filter = fmt.contains("CSV")
        ? QStringLiteral("CSV 文件 (*.csv)")
        : QStringLiteral("Excel 文件 (*.xlsx)");
    QString path = QFileDialog::getSaveFileName(this,
        QStringLiteral("保存文件"),
        QString(), filter);
    if (path.isEmpty()) return;

    try {
        Exporter exporter(m_db);
        bool success = fmt.contains("CSV")
            ? exporter.exportCsv(path)
            : exporter.exportExcel(path);

        if (success) {
            QMessageBox::information(this,
                QStringLiteral("导出成功"),
                QStringLiteral("记录已导出到:\n") + path);
        } else {
            QMessageBox::critical(this,
                QStringLiteral("导出失败"),
                QStringLiteral("无法写入文件"));
        }
    } catch (const std::exception &e) {
        QMessageBox::critical(this,
            QStringLiteral("导出失败"),
            e.what());
    }
}

void MainWindow::onSettings()
{
    SettingsDialog dialog(m_db, this);
    if (dialog.exec() == QDialog::Accepted) {
        refreshData();
        emit settingsChanged();
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    event->ignore();
    hide();
}
