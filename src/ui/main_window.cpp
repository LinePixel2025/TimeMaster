#include "main_window.h"
#include "database/database_manager.h"
#include "ui/settings_dialog.h"
#include "ui/theme_manager.h"
#include "ui/design_tokens.h"
#include "ui/dashboard_layout.h"
#include "ui/hero_card.h"
#include "ui/chart_card.h"
#include "ui/insight_card.h"
#include "ui/topapp_card.h"
#include "ui/rank_card.h"
#include "ui/compare_card.h"
#include "icon/app_icon_provider.h"
#include "export/exporter.h"

#include <QCloseEvent>
#include <QShowEvent>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QInputDialog>
#include <QFileDialog>
#include <QMessageBox>
#include <QWidget>
#include <QApplication>
#include <QLabel>

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

MainWindow::MainWindow(DatabaseManager *db, QWidget *parent)
    : QMainWindow(parent), m_db(db)
{
    setWindowTitle("Time Master");
    setFixedSize(1000, 700);

    bool dark = ThemeManager::instance()->isDark();
    QColor bg = DesignTokens::kBg();
    QColor textStrong = DesignTokens::kTextStrong();
    QColor textMute = DesignTokens::kTextMute();
    QColor accent = DesignTokens::kAccent();
    QColor accentHover = DesignTokens::kAccentHover();
    QColor accentPressed = DesignTokens::kAccentPressed();

    setAutoFillBackground(true);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, bg);
    setPalette(pal);

    m_centralWidget = new QWidget(this);
    m_centralWidget->setObjectName("centralWidget");
    m_centralWidget->setStyleSheet(
        QString("#centralWidget { background-color: %1; }").arg(bg.name()));
    setCentralWidget(m_centralWidget);
    QVBoxLayout *layout = new QVBoxLayout(m_centralWidget);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(20);

    QHBoxLayout *headerLayout = new QHBoxLayout();
    headerLayout->setSpacing(16);

    m_titleLabel = new QLabel("Time Master", this);
    m_titleLabel->setFont(DesignTokens::appFont(20, QFont::Medium));
    m_titleLabel->setStyleSheet(QString("color: %1;").arg(textStrong.name()));
    headerLayout->addWidget(m_titleLabel);

    m_themeBtn = new QPushButton(dark ? QString::fromUtf8("\xe2\x98\x80") : QString::fromUtf8("\xf0\x9f\x8c\x99"), this);
    m_themeBtn->setStyleSheet(
        QString("QPushButton { background-color: transparent; color: %1; border: none; "
                "font-size: 18px; padding: 4px; }"
                "QPushButton:hover { background-color: %2; border-radius: 6px; }")
            .arg(textMute.name(), DesignTokens::kButtonHoverBg().name()));
    m_themeBtn->setToolTip(dark ? QString::fromUtf8("\xe5\x88\x87\xe6\x8d\xa2\xe5\x88\xb0\xe4\xba\xae\xe8\x89\xb2\xe6\xa8\xa1\xe5\xbc\x8f") : QString::fromUtf8("\xe5\x88\x87\xe6\x8d\xa2\xe5\x88\xb0\xe6\x9a\x97\xe8\x89\xb2\xe6\xa8\xa1\xe5\xbc\x8f"));
    connect(m_themeBtn, &QPushButton::clicked, this, []() {
        ThemeManager::instance()->toggle();
    });
    headerLayout->addWidget(m_themeBtn);

    m_settingsBtn = new QPushButton(QString::fromUtf8("\xe2\x9a\x99"), this);
    m_settingsBtn->setStyleSheet(
        QString("QPushButton { background-color: transparent; color: %1; border: none; "
                "font-size: 20px; padding: 4px; }"
                "QPushButton:hover { background-color: %2; border-radius: 6px; }")
            .arg(textMute.name(), DesignTokens::kButtonHoverBg().name()));
    m_settingsBtn->setToolTip(QString::fromUtf8("\xe8\xae\xbe\xe7\xbd\xae"));
    connect(m_settingsBtn, &QPushButton::clicked, this, &MainWindow::onSettings);
    headerLayout->addWidget(m_settingsBtn);

    headerLayout->addStretch();

    const QString btnStyle = QString(
        "QPushButton {"
        "  background-color: %1;"
        "  color: white; border: none; border-radius: 8px;"
        "  padding: 8px 20px; font-size: 13px; font-weight: 500;"
        "}"
        "QPushButton:hover {"
        "  background-color: %2;"
        "}"
        "QPushButton:pressed {"
        "  background-color: %3;"
        "}")
        .arg(accent.name(), accentHover.name(), accentPressed.name());

    m_exportBtn = new QPushButton(QString::fromUtf8("\xe5\xaf\xbc\xe5\x87\xba\xe8\xae\xb0\xe5\xbd\x95"), this);
    m_exportBtn->setStyleSheet(btnStyle);
    connect(m_exportBtn, &QPushButton::clicked, this, &MainWindow::onExport);
    headerLayout->addWidget(m_exportBtn);

    m_refreshBtn = new QPushButton(QString::fromUtf8("\xe5\x88\xb7\xe6\x96\xb0"), this);
    m_refreshBtn->setStyleSheet(btnStyle);
    connect(m_refreshBtn, &QPushButton::clicked, this, &MainWindow::refreshData);
    headerLayout->addWidget(m_refreshBtn);

    layout->addLayout(headerLayout);

    m_contentGrid = new QGridLayout();
    m_contentGrid->setSpacing(16);
    m_contentGrid->setContentsMargins(0, 0, 0, 0);
    layout->addLayout(m_contentGrid, 1);

    loadLayout();

    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setInterval(10000);
    connect(m_refreshTimer, &QTimer::timeout, this, &MainWindow::refreshData);
    m_refreshTimer->start();

    connect(ThemeManager::instance(), &ThemeManager::themeChanged, this, [this](ThemeManager::Theme theme) {
        bool d = (theme == ThemeManager::Dark);
        m_themeBtn->setText(d ? QString::fromUtf8("\xe2\x98\x80") : QString::fromUtf8("\xf0\x9f\x8c\x99"));
        m_themeBtn->setToolTip(d ? QString::fromUtf8("\xe5\x88\x87\xe6\x8d\xa2\xe5\x88\xb0\xe4\xba\xae\xe8\x89\xb2\xe6\xa8\xa1\xe5\xbc\x8f") : QString::fromUtf8("\xe5\x88\x87\xe6\x8d\xa2\xe5\x88\xb0\xe6\x9a\x97\xe8\x89\xb2\xe6\xa8\xa1\xe5\xbc\x8f"));

        QColor bg = DesignTokens::kBg();
        QColor textStrong = DesignTokens::kTextStrong();
        QColor textMute = DesignTokens::kTextMute();
        QColor accent = DesignTokens::kAccent();
        QColor accentHover = DesignTokens::kAccentHover();
        QColor accentPressed = DesignTokens::kAccentPressed();

        QPalette pal = palette();
        pal.setColor(QPalette::Window, bg);
        setPalette(pal);
        m_centralWidget->setStyleSheet(
            QString("#centralWidget { background-color: %1; }").arg(bg.name()));

        m_titleLabel->setStyleSheet(QString("color: %1;").arg(textStrong.name()));

        QString iconBtnQss = QString(
            "QPushButton { background-color: transparent; color: %1; border: none; "
            "font-size: 20px; padding: 4px; }"
            "QPushButton:hover { background-color: %2; border-radius: 6px; }")
            .arg(textMute.name(), DesignTokens::kButtonHoverBg().name());
        m_themeBtn->setStyleSheet(iconBtnQss);
        m_settingsBtn->setStyleSheet(iconBtnQss);

        QString btnStyle = QString(
            "QPushButton {"
            "  background-color: %1;"
            "  color: white; border: none; border-radius: 8px;"
            "  padding: 8px 20px; font-size: 13px; font-weight: 500;"
            "}"
            "QPushButton:hover { background-color: %2; }"
            "QPushButton:pressed { background-color: %3; }")
            .arg(accent.name(), accentHover.name(), accentPressed.name());
        m_exportBtn->setStyleSheet(btnStyle);
        m_refreshBtn->setStyleSheet(btnStyle);

        if (isVisible()) {
            HWND hwnd = reinterpret_cast<HWND>(winId());
            applyDwmTitleBar(hwnd, d);
        }
    });
}

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    applyDwmTitleBar(reinterpret_cast<HWND>(winId()), ThemeManager::instance()->isDark());
}

void MainWindow::refreshData()
{
    QVector<QVariantMap> weekData = m_db->getWeekSummary();
    QVector<QVariantMap> rankData = m_db->getAppRank();
    int todayTotal = m_db->getTodayTotal();
    int yesterdayTotal = m_db->getYesterdayTotal();
    int dailyGoal = m_db->getSetting("daily_goal", "28800").toInt();

    if (auto *w = m_cards.value("today_total")) {
        if (auto *c = qobject_cast<HeroCard*>(w))
            c->setData(todayTotal, yesterdayTotal, dailyGoal, {});
    }
    if (auto *w = m_cards.value("weekly_chart")) {
        if (auto *c = qobject_cast<ChartCard*>(w))
            c->setData(weekData);
    }
    if (auto *w = m_cards.value("top_app")) {
        if (auto *c = qobject_cast<TopAppCard*>(w)) {
            if (!rankData.isEmpty()) {
                QIcon icon = AppIconProvider::instance()->icon(
                    rankData[0]["process_name"].toString(), 24);
                c->setApp(rankData[0]["app_name"].toString(),
                         rankData[0]["total_seconds"].toInt(), icon);
            }
        }
    }
    if (auto *w = m_cards.value("app_ranking")) {
        if (auto *c = qobject_cast<RankCard*>(w))
            c->refresh(rankData);
    }
    if (auto *w = m_cards.value("yesterday_compare")) {
        if (auto *c = qobject_cast<CompareCard*>(w))
            c->setData(todayTotal, yesterdayTotal);
    }
}

void MainWindow::loadLayout()
{
    clearLayout();
    QString json = m_db->getSetting("dashboard_layout");
    QVector<DashboardLayoutItem> items = DashboardLayoutParser::parse(json);

    int maxRow = 0;
    for (const auto &item : items) {
        if (item.visible && item.row > maxRow)
            maxRow = item.row;
    }

    for (const auto &item : items) {
        if (!item.visible)
            continue;
        QWidget *card = createCard(item.id);
        if (card) {
            m_contentGrid->addWidget(card, item.row, item.col,
                                    1, qBound(1, item.colSpan, 2));
            m_cards[item.id] = card;
        }
    }

    refreshData();
}

void MainWindow::clearLayout()
{
    m_cards.clear();
    while (m_contentGrid->count() > 0) {
        QLayoutItem *item = m_contentGrid->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }
}

QWidget *MainWindow::createCard(const QString &cardId)
{
    if (cardId == "today_total") {
        return new HeroCard(this);
    } else if (cardId == "weekly_chart") {
        auto *c = new ChartCard(this);
        c->setChartType(m_db->getSetting("chart_type", "bar"));
        return c;
    } else if (cardId == "ai_insight") {
        auto *c = new InsightCard(this);
        c->setConfigured(m_db->getSetting("lineweb_enabled", "false") == "true");
        return c;
    } else if (cardId == "top_app") {
        return new TopAppCard(this);
    } else if (cardId == "app_ranking") {
        return new RankCard(this);
    } else if (cardId == "yesterday_compare") {
        return new CompareCard(this);
    }
    return nullptr;
}

void MainWindow::onExport()
{
    QStringList formats;
    formats << QString::fromUtf8("CSV (.csv)")
            << QString::fromUtf8("Excel (.xlsx)");
    bool ok = false;
    QString fmt = QInputDialog::getItem(this,
        QString::fromUtf8("\xe5\xaf\xbc\xe5\x87\xba\xe6\xa0\xbc\xe5\xbc\x8f"),
        QString::fromUtf8("\xe9\x80\x89\xe6\x8b\xa9\xe5\xaf\xbc\xe5\x87\xba\xe6\xa0\xbc\xe5\xbc\x8f:"),
        formats, 0, false, &ok);
    if (!ok) return;

    QString filter = fmt.contains("CSV")
        ? QString::fromUtf8("CSV \xe6\x96\x87\xe4\xbb\xb6 (*.csv)")
        : QString::fromUtf8("Excel \xe6\x96\x87\xe4\xbb\xb6 (*.xlsx)");
    QString path = QFileDialog::getSaveFileName(this,
        QString::fromUtf8("\xe4\xbf\x9d\xe5\xad\x98\xe6\x96\x87\xe4\xbb\xb6"),
        "", filter);
    if (path.isEmpty()) return;

    try {
        Exporter exporter(m_db);
        bool success = false;
        if (fmt.contains("CSV"))
            success = exporter.exportCsv(path);
        else
            success = exporter.exportExcel(path);

        if (success) {
            QMessageBox::information(this,
                QString::fromUtf8("\xe5\xaf\xbc\xe5\x87\xba\xe6\x88\x90\xe5\x8a\x9f"),
                QString::fromUtf8("\xe8\xae\xb0\xe5\xbd\x95\xe5\xb7\xb2\xe5\xaf\xbc\xe5\x87\xba\xe5\x88\xb0:\n") + path);
        } else {
            QMessageBox::critical(this,
                QString::fromUtf8("\xe5\xaf\xbc\xe5\x87\xba\xe5\xa4\xb1\xe8\xb4\xa5"),
                QString::fromUtf8("\xe6\x97\xa0\xe6\xb3\x95\xe5\x86\x99\xe5\x85\xa5\xe6\x96\x87\xe4\xbb\xb6"));
        }
    } catch (const std::exception &e) {
        QMessageBox::critical(this,
            QString::fromUtf8("\xe5\xaf\xbc\xe5\x87\xba\xe5\xa4\xb1\xe8\xb4\xa5"),
            e.what());
    }
}

void MainWindow::onSettings()
{
    SettingsDialog dialog(m_db, this);
    if (dialog.exec() == QDialog::Accepted) {
        loadLayout();
        emit settingsChanged();
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    event->ignore();
    hide();
}
