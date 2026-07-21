### Task 9: MainWindow

**Files:**
- Create: `src/ui/main_window.h`
- Create: `src/ui/main_window.cpp`

**Interfaces:**
- Consumes: `DatabaseManager`, `StatsWidget`, `AppRankWidget`, `Exporter`, Qt6::Widgets
- Produces: `MainWindow` with Mica/acrylic backdrop

- [ ] **Step 1: Write main_window.h**

```cpp
#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include <QMainWindow>
#include <QTimer>

class DatabaseManager;
class StatsWidget;
class AppRankWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(DatabaseManager *db, QWidget *parent = nullptr);

public slots:
    void refreshData();

protected:
    void showEvent(QShowEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private slots:
    void applyBackdrop();
    void onExport();

private:
    DatabaseManager *m_db;
    StatsWidget *m_statsWidget;
    AppRankWidget *m_appRankWidget;
    QTimer *m_refreshTimer;
};

#endif // MAIN_WINDOW_H
```

- [ ] **Step 2: Write main_window.cpp**

```cpp
#include "main_window.h"
#include "database/database_manager.h"
#include "ui/stats_widget.h"
#include "ui/app_rank_widget.h"
#include "export/exporter.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QInputDialog>
#include <QFileDialog>
#include <QMessageBox>
#include <QWidget>
#include <QApplication>

#include <windows.h>
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")

static const DWORD DWMWA_USE_IMMERSIVE_DARK_MODE = 20;
static const DWORD DWMWA_SYSTEMBACKDROP_TYPE = 38;
static const int MICA = 2;
static const int DWMSBT_ACRYLICWINDOW = 3;

static DWORD getWindowsBuild()
{
    using RtlGetVersionPtr = LONG(WINAPI*)(PRTL_OSVERSIONINFOW);
    HMODULE ntdll = GetModuleHandleW(L"ntdll");
    if (!ntdll) return 0;
    auto RtlGetVersion = reinterpret_cast<RtlGetVersionPtr>(
        GetProcAddress(ntdll, "RtlGetVersion"));
    if (!RtlGetVersion) return 0;

    RTL_OSVERSIONINFOW osvi = {};
    osvi.dwOSVersionInfoSize = sizeof(osvi);
    if (RtlGetVersion(&osvi) == 0)
        return osvi.dwBuildNumber;
    return 0;
}

static void applyBackdropEffect(HWND hwnd)
{
    DWORD build = getWindowsBuild();
    int backdrop = (build >= 22000) ? MICA : DWMSBT_ACRYLICWINDOW;
    DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &backdrop, sizeof(backdrop));
    BOOL dark = TRUE;
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
}

MainWindow::MainWindow(DatabaseManager *db, QWidget *parent)
    : QMainWindow(parent), m_db(db)
{
    setWindowTitle("Time Master");
    setMinimumSize(800, 500);
    resize(900, 600);
    setAttribute(Qt::WA_TranslucentBackground);
    setStyleSheet(
        "QPushButton {"
        "  background-color: rgba(99, 102, 241, 200);"
        "  color: white; border: none; border-radius: 8px;"
        "  padding: 8px 20px; font-family: 'PingFang SC'; font-size: 13px;"
        "}"
        "QPushButton:hover {"
        "  background-color: rgba(129, 140, 248, 220);"
        "}"
        "QPushButton:pressed {"
        "  background-color: rgba(79, 70, 229, 220);"
        "}"
    );

    QWidget *central = new QWidget(this);
    setCentralWidget(central);
    QVBoxLayout *layout = new QVBoxLayout(central);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(16);

    m_statsWidget = new StatsWidget(db, this);
    layout->addWidget(m_statsWidget);

    m_appRankWidget = new AppRankWidget(db, this);
    layout->addWidget(m_appRankWidget);

    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();

    QPushButton *exportBtn = new QPushButton(QString::fromUtf8("\xf0\x9f\x93\xa4 \xe5\xaf\xbc\xe5\x87\xba\xe8\xae\xb0\xe5\xbd\x95"), this);
    connect(exportBtn, &QPushButton::clicked, this, &MainWindow::onExport);
    btnLayout->addWidget(exportBtn);

    QPushButton *refreshBtn = new QPushButton(QString::fromUtf8("\xf0\x9f\x94\x84 \xe5\x88\xb7\xe6\x96\xb0"), this);
    connect(refreshBtn, &QPushButton::clicked, this, &MainWindow::refreshData);
    btnLayout->addWidget(refreshBtn);

    layout->addLayout(btnLayout);

    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setInterval(10000);
    connect(m_refreshTimer, &QTimer::timeout, this, &MainWindow::refreshData);
    m_refreshTimer->start();
}

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    QTimer::singleShot(100, this, &MainWindow::applyBackdrop);
}

void MainWindow::applyBackdrop()
{
    HWND hwnd = reinterpret_cast<HWND>(winId());
    applyBackdropEffect(hwnd);
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

void MainWindow::closeEvent(QCloseEvent *event)
{
    event->ignore();
    hide();
}
```

- [ ] **Step 3: Commit**

```bash
git add -A
git commit -m "feat: MainWindow with Mica backdrop and widget integration"
```

---

