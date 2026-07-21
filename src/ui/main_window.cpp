#include "main_window.h"
#include "database/database_manager.h"
#include "ui/stats_widget.h"
#include "ui/app_rank_widget.h"
#include "ui/settings_dialog.h"
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

static QFont appFont(int size, QFont::Weight weight = QFont::Normal)
{
    QStringList families = {"Microsoft YaHei", "Segoe UI", "PingFang SC"};
    QFont font;
    for (const auto &f : families) {
        font = QFont(f, size, weight);
        if (QFont(f).exactMatch())
            return font;
    }
    font.setPixelSize(size * 1.4);
    font.setWeight(weight);
    return font;
}

void MainWindow::setupPalette()
{
    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor("#F0F2F5"));
    setPalette(pal);
}

MainWindow::MainWindow(DatabaseManager *db, QWidget *parent)
    : QMainWindow(parent), m_db(db)
{
    setWindowTitle("Time Master");
    setMinimumSize(900, 600);
    resize(1000, 700);

    setAutoFillBackground(true);
    setupPalette();

    m_centralWidget = new QWidget(this);
    m_centralWidget->setObjectName("centralWidget");
    m_centralWidget->setStyleSheet("#centralWidget { background-color: #F0F2F5; }");
    setCentralWidget(m_centralWidget);
    QVBoxLayout *layout = new QVBoxLayout(m_centralWidget);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(20);

    QHBoxLayout *headerLayout = new QHBoxLayout();
    headerLayout->setSpacing(16);
    QLabel *titleLabel = new QLabel("Time Master", this);
    titleLabel->setFont(appFont(20, QFont::Medium));
    titleLabel->setStyleSheet("color: #1F2937;");
    headerLayout->addWidget(titleLabel);

    QPushButton *settingsBtn = new QPushButton(QString::fromUtf8("\xe2\x9a\x99"), this);
    settingsBtn->setStyleSheet(
        "QPushButton { background-color: transparent; color: #4B5563; border: none; "
        "font-size: 20px; padding: 4px; }"
        "QPushButton:hover { background-color: #E5E7EB; border-radius: 6px; }");
    settingsBtn->setToolTip(QString::fromUtf8("\xe8\xae\xbe\xe7\xbd\xae"));
    connect(settingsBtn, &QPushButton::clicked, this, &MainWindow::onSettings);
    headerLayout->addWidget(settingsBtn);

    headerLayout->addStretch();

    const QString btnStyle =
        "QPushButton {"
        "  background-color: #6366F1;"
        "  color: white; border: none; border-radius: 8px;"
        "  padding: 8px 20px; font-size: 13px; font-weight: 500;"
        "}"
        "QPushButton:hover {"
        "  background-color: #818CF8;"
        "}"
        "QPushButton:pressed {"
        "  background-color: #4F46E5;"
        "}";

    QPushButton *exportBtn = new QPushButton(QString::fromUtf8("\xe5\xaf\xbc\xe5\x87\xba\xe8\xae\xb0\xe5\xbd\x95"), this);
    exportBtn->setStyleSheet(btnStyle);
    connect(exportBtn, &QPushButton::clicked, this, &MainWindow::onExport);
    headerLayout->addWidget(exportBtn);

    QPushButton *refreshBtn = new QPushButton(QString::fromUtf8("\xe5\x88\xb7\xe6\x96\xb0"), this);
    refreshBtn->setStyleSheet(btnStyle);
    connect(refreshBtn, &QPushButton::clicked, this, &MainWindow::refreshData);
    headerLayout->addWidget(refreshBtn);

    layout->addLayout(headerLayout);

    m_statsWidget = new StatsWidget(db, this);
    layout->addWidget(m_statsWidget);

    QHBoxLayout *rankRow = new QHBoxLayout();
    rankRow->setSpacing(16);
    m_appRankWidget = new AppRankWidget(db, this);
    m_appRankWidget->setMaximumHeight(280);
    m_appRankWidget->setMinimumHeight(120);
    rankRow->addWidget(m_appRankWidget, 1);
    rankRow->addStretch(1);
    layout->addLayout(rankRow);

    layout->addStretch();

    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setInterval(10000);
    connect(m_refreshTimer, &QTimer::timeout, this, &MainWindow::refreshData);
    m_refreshTimer->start();
}

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    HWND hwnd = reinterpret_cast<HWND>(winId());
    BOOL dark = FALSE;
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
    COLORREF color = RGB(0xF0, 0xF2, 0xF5);
    DwmSetWindowAttribute(hwnd, DWMWA_CAPTION_COLOR, &color, sizeof(color));
}

void MainWindow::refreshData()
{
    m_statsWidget->refresh();
    m_appRankWidget->refresh();
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
        refreshData();
        emit settingsChanged();
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    event->ignore();
    hide();
}
