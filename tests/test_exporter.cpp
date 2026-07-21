#include <cassert>
#include <iostream>
#include <QCoreApplication>
#include <QTemporaryFile>
#include <QDir>
#include <QDateTime>
#include "database/database_manager.h"
#include "export/exporter.h"

void test_export_csv()
{
    QTemporaryFile tmpFile;
    assert(tmpFile.open());
    QString dbPath = tmpFile.fileName();
    tmpFile.close();

    DatabaseManager db(dbPath);
    QDateTime now = QDateTime::currentDateTime();
    db.insertSession("chrome.exe", "test", "Chrome", now, now, 60);

    Exporter exporter(&db);
    QString csvPath = QDir::tempPath() + "/test_export.csv";
    bool ok = exporter.exportCsv(csvPath);
    assert(ok);

    QFile file(csvPath);
    assert(file.open(QIODevice::ReadOnly));
    QByteArray content = file.readAll();
    file.close();
    QDir().remove(csvPath);

    assert(content.contains("Chrome"));
    std::cout << "test_export_csv PASS" << std::endl;
}

void test_export_excel()
{
    QTemporaryFile tmpFile;
    assert(tmpFile.open());
    QString dbPath = tmpFile.fileName();
    tmpFile.close();

    DatabaseManager db(dbPath);
    QDateTime now = QDateTime::currentDateTime();
    db.insertSession("Code.exe", "test.py", "VS Code", now, now, 120);

    Exporter exporter(&db);
    QString xlsxPath = QDir::tempPath() + "/test_export.xlsx";
    bool ok = exporter.exportExcel(xlsxPath);
    assert(ok);
    assert(QFile::exists(xlsxPath));
    QDir().remove(xlsxPath);
    std::cout << "test_export_excel PASS" << std::endl;
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    test_export_csv();
    test_export_excel();
    std::cout << "All exporter tests passed!" << std::endl;
    return 0;
}
