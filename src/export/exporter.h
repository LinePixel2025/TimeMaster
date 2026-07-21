#ifndef EXPORTER_H
#define EXPORTER_H

#include <QString>

class DatabaseManager;

class Exporter
{
public:
    explicit Exporter(DatabaseManager *db);
    bool exportCsv(const QString &path);
    bool exportExcel(const QString &path);

private:
    DatabaseManager *m_db;
};

#endif // EXPORTER_H
