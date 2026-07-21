#ifndef XLSX_WRITER_H
#define XLSX_WRITER_H

#include <QString>
#include <QVector>
#include <QStringList>
#include <QVariant>
#include <QMap>

class XlsxWriter
{
public:
    XlsxWriter();
    ~XlsxWriter();

    void addSheet(const QString &name);
    void setHeaders(const QStringList &headers);
    void addRow(const QVector<QVariant> &row);
    void setColumnWidths(const QVector<double> &widths);
    bool save(const QString &path);

private:
    struct Sheet {
        QString name;
        QStringList headers;
        QVector<QVector<QVariant>> rows;
        QVector<double> columnWidths;
    };

    int getOrCreateSharedStringIndex(const QString &str);
    QString escapeXml(const QString &str);
    QByteArray buildContentTypes();
    QByteArray buildRels();
    QByteArray buildWorkbook();
    QByteArray buildWorkbookRels();
    QByteArray buildSheet(const Sheet &sheet, int index);
    QByteArray buildSharedStrings();

    QVector<Sheet> m_sheets;
    QMap<QString, int> m_sharedStrings;
    QVector<QString> m_sharedStringsList;
};

#endif // XLSX_WRITER_H
