#include "exporter.h"
#include "database/database_manager.h"
#include "export/xlsx_writer.h"

#include <QFile>
#include <QTextStream>
#include <QDate>
#include <QDebug>

Exporter::Exporter(DatabaseManager *db)
    : m_db(db)
{
}

bool Exporter::exportCsv(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    QTextStream out(&file);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    out.setCodec("UTF-8");
#endif
    // UTF-8 BOM
    out << QChar(0xFEFF);

    out << QString::fromUtf8("\xe8\xbf\x9b\xe7\xa8\x8b\xe5\x90\x8d")
        << "," << QString::fromUtf8("\xe7\xaa\x97\xe5\x8f\xa3\xe6\xa0\x87\xe9\xa2\x98")
        << "," << QString::fromUtf8("\xe5\xba\x94\xe7\x94\xa8\xe5\x90\x8d")
        << "," << QString::fromUtf8("\xe5\xbc\x80\xe5\xa7\x8b\xe6\x97\xb6\xe9\x97\xb4")
        << "," << QString::fromUtf8("\xe7\xbb\x93\xe6\x9d\x9f\xe6\x97\xb6\xe9\x97\xb4")
        << "," << QString::fromUtf8("\xe6\x8c\x81\xe7\xbb\xad\xe7\xa7\x92\xe6\x95\xb0")
        << "\n";

    auto sessions = m_db->getAllSessions();
    for (const auto &s : sessions) {
        auto csvEscape = [](const QString &val) {
            if (val.contains(',') || val.contains('"') || val.contains('\n')) {
                QString escaped = val;
                escaped.replace('"', "\"\"");
                return '"' + escaped + '"';
            }
            return val;
        };
        out << csvEscape(s["process_name"].toString()) << ","
            << csvEscape(s["window_title"].toString()) << ","
            << csvEscape(s["app_name"].toString()) << ","
            << s["start_time"].toString() << ","
            << s["end_time"].toString() << ","
            << s["duration_seconds"].toString() << "\n";
    }

    file.close();
    return true;
}

bool Exporter::exportExcel(const QString &path)
{
    XlsxWriter xlsx;

    // Sheet 1: Usage Records
    xlsx.addSheet(QString::fromUtf8("\xe4\xbd\xbf\xe7\x94\xa8\xe8\xae\xb0\xe5\xbd\x95"));
    xlsx.setHeaders({
        QString::fromUtf8("\xe8\xbf\x9b\xe7\xa8\x8b\xe5\x90\x8d"),
        QString::fromUtf8("\xe7\xaa\x97\xe5\x8f\xa3\xe6\xa0\x87\xe9\xa2\x98"),
        QString::fromUtf8("\xe5\xba\x94\xe7\x94\xa8\xe5\x90\x8d"),
        QString::fromUtf8("\xe5\xbc\x80\xe5\xa7\x8b\xe6\x97\xb6\xe9\x97\xb4"),
        QString::fromUtf8("\xe7\xbb\x93\xe6\x9d\x9f\xe6\x97\xb6\xe9\x97\xb4"),
        QString::fromUtf8("\xe6\x8c\x81\xe7\xbb\xad\xe7\xa7\x92\xe6\x95\xb0")
    });
    xlsx.setColumnWidths({20, 30, 15, 22, 22, 12});

    auto sessions = m_db->getAllSessions();
    for (const auto &s : sessions) {
        xlsx.addRow({
            s["process_name"].toString(),
            s["window_title"].toString(),
            s["app_name"].toString(),
            s["start_time"].toString(),
            s["end_time"].toString(),
            s["duration_seconds"].toInt()
        });
    }

    // Sheet 2: Daily Summary
    QDate today = QDate::currentDate();
    QDate monday = today.addDays(-today.dayOfWeek() + 1);
    xlsx.addSheet(QString::fromUtf8("\xe6\xaf\x8f\xe6\x97\xa5\xe6\xb1\x87\xe6\x80\xbb"));
    xlsx.setHeaders({
        QString::fromUtf8("\xe6\x97\xa5\xe6\x9c\x9f"),
        QString::fromUtf8("\xe5\xba\x94\xe7\x94\xa8\xe5\x90\x8d"),
        QString::fromUtf8("\xe4\xbd\xbf\xe7\x94\xa8\xe7\xa7\x92\xe6\x95\xb0")
    });
    xlsx.setColumnWidths({15, 20, 12});

    auto summaries = m_db->getDailySummaries(monday.toString(Qt::ISODate), today.toString(Qt::ISODate));
    for (const auto &s : summaries) {
        xlsx.addRow({s["d"].toString(), s["app_name"].toString(), s["total_seconds"].toInt()});
    }

    // Sheet 3: App Ranking
    xlsx.addSheet(QString::fromUtf8("\xe5\xba\x94\xe7\x94\xa8\xe6\x8e\x92\xe8\xa1\x8c"));
    xlsx.setHeaders({
        QString::fromUtf8("\xe5\xba\x94\xe7\x94\xa8\xe5\x90\x8d"),
        QString::fromUtf8("\xe4\xbd\xbf\xe7\x94\xa8\xe7\xa7\x92\xe6\x95\xb0")
    });
    xlsx.setColumnWidths({20, 12});

    auto rank = m_db->getAppRank();
    for (const auto &r : rank) {
        xlsx.addRow({r["app_name"].toString(), r["total_seconds"].toInt()});
    }

    return xlsx.save(path);
}
