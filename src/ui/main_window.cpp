#include "ui/main_window.h"
#include "database/database_manager.h"
#include "ui/settings_dialog.h"
#include "ui/theme_manager.h"
#include "ui/design_tokens.h"
#include "ui/hero_card.h"
#include "ui/trend_card.h"
#include "ui/rank_card.h"
#include "ui/compare_card.h"
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
    setMinimumSize(960, 640);
    resize(1100, 720);

    QColor bg = DesignTokens::kBg();
    QColor textStrong = DesignTokens::kTextStrong();
    QColor textMute = DesignTokens::kTextMute();
    QColor accent = DesignTokens::kAccent();
    QColor accentHover = DesignTokens::kAccentHover();
    QColor accentPressed = DesignTokens::kAccentPressed();

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

    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(24, 20, 24, 24);
    layout->setSpacing(16);

    // ---- Header ----
    auto *headerLayout = new QHBoxLayout();
    headerLayout->setSpacing(10);

    auto *titleLabel = new QLabel("Time Master", central);
    titleLabel->setFont(DesignTokens::appFont(19, QFont::Medium));
    titleLabel->setStyleSheet(QString("color: %1; background: transparent;").arg(textStrong.name()));
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();

    const QString iconBtnQss = QString(
        "QPushButton { background-color: transparent; color: %1; border: none;"
        " font-size: 17px; padding: 4px 8px; border-radius: 6px; }"
        "QPushButton:hover { background-color: %2; }")
        .arg(textMute.name(), DesignTokens::kButtonHoverBg().name());

    m_themeBtn = new QPushButton(
        ThemeManager::instance()->isDark()
            ? QString::fromUtf8("\xe2\x98\x80")
            : QString::fromUtf8("\xf0\x9f\x8c\x99"),
        central);
    m_themeBtn->setStyleSheet(iconBtnQss);
    m_themeBtn->setToolTip(
        ThemeManager::instance()->isDark()
            ? QString::fromUtf8("\xe5\x88\x87\xe6\x8d\xa2\xe5\x88\xb0\xe4\xba\xae\xe8\x89\xb2\xe6\xa8\xa1\xe5\xbc\x8f")
            : QString::fromUtf8("\xe5\x88\x87\xe6\x8d\xa2\xe5\x88\xb0\xe6\x9a\x97\xe8\x89\xb2\xe6\xa8\xa1\xe5\xbc\x8f"));
    connect(m_themeBtn, &QPushButton::clicked, this, []() {
        ThemeManager::instance()->toggle();
    });
    headerLayout->addWidget(m_themeBtn);

    m_settingsBtn = new QPushButton(QString::fromUtf8("\xe2\x9a\x99"), central);
    m_settingsBtn->setStyleSheet(iconBtnQss);
    m_settingsBtn->setToolTip(QString::fromUtf8("\xe8\xae\xbe\xe7\xbd\xae"));
    connect(m_settingsBtn, &QPushButton::clicked, this, &MainWindow::onSettings);
    headerLayout->addWidget(m_settingsBtn);

    const QString primaryBtnQss = QString(
        "QPushButton { background-color: %1; color: white; border: none;"
        " border-radius: 8px; padding: 8px 18px; font-size: 13px; font-weight: 500; }"
        "QPushButton:hover { background-color: %2; }"
        "QPushButton:pressed { background-color: %3; }")
        .arg(accent.name(), accentHover.name(), accentPressed.name());

    m_exportBtn = new QPushButton(QString::fromUtf8("\xe5\xaf\xbc\xe5\x87\xba\xe8\xae\xb0\xe5\xbd\x95"), central);
    m_exportBtn->setStyleSheet(primaryBtnQss);
    connect(m_exportBtn, &QPushButton::clicked, this, &MainWindow::onExport);
    headerLayout->addWidget(m_exportBtn);

    m_refreshBtn = new QPushButton(QString::fromUtf8("\xe5\x88\xb7\xe6\x96\xb0"), central);
    m_refreshBtn->setStyleSheet(primaryBtnQss);
    connect(m_refreshBtn, &QPushButton::clicked, this, &MainWindow::refreshData);
    headerLayout->addWidget(m_refreshBtn);

    layout->addLayout(headerLayout);

    // ---- Cards: 2x2 grid ----
    auto *grid = new QGridLayout();
    grid->setSpacing(16);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);

    m_heroCard = new HeroCard(central);
    m_trendCard = new TrendCard(central);
    m_compareCard = new CompareCard(central);
    m_rankCard = new RankCard(central);

    grid->addWidget(m_heroCard, 0, 0);
    grid->addWidget(m_trendCard, 0, 1);
    grid->addWidget(m_compareCard, 1, 0);
    grid->addWidget(m_rankCard, 1, 1);
    grid->setRowStretch(0, 5);
    grid->setRowStretch(1, 5);

    layout->addLayout(grid, 1);

    // Persist chart type when the user toggles it.
    connect(m_trendCard, &TrendCard::chartTypeChanged, this,
            [this](const QString &type) {
        m_db->setSetting("chart_type", type);
    });

    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setInterval(10000);
    connect(m_refreshTimer, &QTimer::timeout, this, &MainWindow::refreshData);
    m_refreshTimer->start();

    // ---- Theme change handling ----
    connect(ThemeManager::instance(), &ThemeManager::themeChanged,
            this, [this](ThemeManager::Theme theme) {
        const bool dark = (theme == ThemeManager::Dark);
        m_themeBtn->setText(dark ? QString::fromUtf8("\xe2\x98\x80")
                                 : QString::fromUtf8("\xf0\x9f\x8c\x99"));
        m_themeBtn->setToolTip(
            dark
                ? QString::fromUtf8("\xe5\x88\x87\xe6\x8d\xa2\xe5\x88\xb0\xe4\xba\xae\xe8\x89\xb2\xe6\xa8\xa1\xe5\xbc\x8f")
                : QString::fromUtf8("\xe5\x88\x87\xe6\x8d\xa2\xe5\x88\xb0\xe6\x9a\x97\xe8\x89\xb2\xe6\xa8\xa1\xe5\xbc\x8f"));

        QColor bg = DesignTokens::kBg();
        QColor textStrong = DesignTokens::kTextStrong();
        QColor textMute = DesignTokens::kTextMute();

        QPalette pal = palette();
        pal.setColor(QPalette::Window, bg);
        pal.setColor(QPalette::Base, DesignTokens::kSurface());
        pal.setColor(QPalette::Text, textStrong);
        pal.setColor(QPalette::WindowText, textStrong);
        setPalette(pal);

        centralWidget()->setStyleSheet(
            QString("#centralWidget { background-color: %1; }").arg(bg.name()));

        auto *titleLabel = qobject_cast<QLabel*>(
            centralWidget()->layout()->itemAt(0)->layout()->itemAt(0)->widget());
        if (titleLabel) {
            titleLabel->setStyleSheet(
                QString("color: %1; background: transparent;").arg(textStrong.name()));
        }

        const QString iconBtnQss = QString(
            "QPushButton { background-color: transparent; color: %1; border: none;"
            " font-size: 17px; padding: 4px 8px; border-radius: 6px; }"
            "QPushButton:hover { background-color: %2; }")
            .arg(textMute.name(), DesignTokens::kButtonHoverBg().name());
        m_themeBtn->setStyleSheet(iconBtnQss);
        m_settingsBtn->setStyleSheet(iconBtnQss);

        const QString primaryBtnQss = QString(
            "QPushButton { background-color: %1; color: white; border: none;"
            " border-radius: 8px; padding: 8px 18px; font-size: 13px; font-weight: 500; }"
            "QPushButton:hover { background-color: %2; }"
            "QPushButton:pressed { background-color: %3; }")
            .arg(DesignTokens::kAccent().name(),
                 DesignTokens::kAccentHover().name(),
                 DesignTokens::kAccentPressed().name());
        m_exportBtn->setStyleSheet(primaryBtnQss);
        m_refreshBtn->setStyleSheet(primaryBtnQss);

        if (isVisible()) {
            applyDwmTitleBar(reinterpret_cast<HWND>(winId()), dark);
        }
    });

    refreshData();
}

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    applyDwmTitleBar(reinterpret_cast<HWND>(winId()),
                     ThemeManager::instance()->isDark());
}

void MainWindow::refreshData()
{
    const int todayTotal = m_db->getTodayTotal();
    const int yesterdayTotal = m_db->getYesterdayTotal();
    const int dailyGoal = m_db->getSetting("daily_goal", "28800").toInt();

    m_heroCard->setData(todayTotal, yesterdayTotal, dailyGoal);
    m_trendCard->setData(m_db->getWeekSummary());
    m_rankCard->refresh(m_db->getAppRank());
    m_compareCard->setData(todayTotal, yesterdayTotal);
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
        QString(), filter);
    if (path.isEmpty()) return;

    try {
        Exporter exporter(m_db);
        bool success = fmt.contains("CSV")
            ? exporter.exportCsv(path)
            : exporter.exportExcel(path);

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
        refreshData();
        emit settingsChanged();
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    event->ignore();
    hide();
}
