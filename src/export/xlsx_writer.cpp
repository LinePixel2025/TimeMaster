#include "xlsx_writer.h"
#include "miniz/miniz.h"

#include <QFile>
#include <QDebug>

XlsxWriter::XlsxWriter() {}

XlsxWriter::~XlsxWriter() {}

void XlsxWriter::addSheet(const QString &name)
{
    Sheet s;
    s.name = name;
    m_sheets.append(s);
}

void XlsxWriter::setHeaders(const QStringList &headers)
{
    if (!m_sheets.isEmpty())
        m_sheets.last().headers = headers;
}

void XlsxWriter::addRow(const QVector<QVariant> &row)
{
    if (!m_sheets.isEmpty())
        m_sheets.last().rows.append(row);
}

void XlsxWriter::setColumnWidths(const QVector<double> &widths)
{
    if (!m_sheets.isEmpty())
        m_sheets.last().columnWidths = widths;
}

int XlsxWriter::getOrCreateSharedStringIndex(const QString &str)
{
    auto it = m_sharedStrings.constFind(str);
    if (it != m_sharedStrings.constEnd())
        return it.value();
    int idx = m_sharedStringsList.size();
    m_sharedStringsList.append(str);
    m_sharedStrings[str] = idx;
    return idx;
}

QString XlsxWriter::escapeXml(const QString &str)
{
    QString escaped = str;
    escaped.replace('&', "&amp;");
    escaped.replace('<', "&lt;");
    escaped.replace('>', "&gt;");
    escaped.replace('"', "&quot;");
    escaped.replace('\'', "&apos;");
    return escaped;
}

QByteArray XlsxWriter::buildContentTypes()
{
    QByteArray xml = R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">
  <Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>
  <Default Extension="xml" ContentType="application/xml"/>
  <Override PartName="/xl/workbook.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml"/>
  <Override PartName="/xl/sharedStrings.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.sharedStrings+xml"/>
)";
    for (int i = 0; i < m_sheets.size(); ++i) {
        xml += QByteArray("  <Override PartName=\"/xl/worksheets/sheet")
            + QByteArray::number(i + 1)
            + ".xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>\n";
    }
    xml += "</Types>";
    return xml;
}

QByteArray XlsxWriter::buildRels()
{
    return R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
  <Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="xl/workbook.xml"/>
</Relationships>)";
}

QByteArray XlsxWriter::buildWorkbook()
{
    QByteArray xml = R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<workbook xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main"
          xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships">
  <sheets>
)";
    for (int i = 0; i < m_sheets.size(); ++i) {
        xml += QByteArray("    <sheet name=\"")
            + escapeXml(m_sheets[i].name).toUtf8()
            + "\" sheetId=\"" + QByteArray::number(i + 1)
            + "\" r:id=\"rId" + QByteArray::number(i + 1) + "\"/>\n";
    }
    xml += "  </sheets>\n</workbook>";
    return xml;
}

QByteArray XlsxWriter::buildWorkbookRels()
{
    QByteArray xml = R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
)";
    for (int i = 0; i < m_sheets.size(); ++i) {
        xml += QByteArray("  <Relationship Id=\"rId")
            + QByteArray::number(i + 1)
            + "\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet"
            + QByteArray::number(i + 1) + ".xml\"/>\n";
    }
    xml += R"(  <Relationship Id="rIdSharedStrings" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/sharedStrings" Target="sharedStrings.xml"/>
</Relationships>)";
    return xml;
}

QByteArray XlsxWriter::buildSharedStrings()
{
    QByteArray xml = R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<sst xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main" count=")"
        + QByteArray::number(m_sharedStringsList.size())
        + "\" uniqueCount=\"" + QByteArray::number(m_sharedStringsList.size()) + "\">";
    for (const auto &str : m_sharedStringsList) {
        xml += "<si><t>" + escapeXml(str).toUtf8() + "</t></si>";
    }
    xml += "</sst>";
    return xml;
}

static QByteArray toExcelColumn(int col)
{
    QByteArray result;
    while (col >= 0) {
        result.prepend(static_cast<char>('A' + col % 26));
        col = col / 26 - 1;
    }
    return result;
}

QByteArray XlsxWriter::buildSheet(const Sheet &sheet, int index)
{
    Q_UNUSED(index);
    QByteArray xml = R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<worksheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main">
)";
    if (!sheet.columnWidths.isEmpty()) {
        xml += "  <cols>\n";
        for (int i = 0; i < sheet.columnWidths.size(); ++i) {
            xml += QByteArray("    <col min=\"") + QByteArray::number(i + 1)
                + "\" max=\"" + QByteArray::number(i + 1)
                + "\" width=\"" + QByteArray::number(sheet.columnWidths[i], 'f', 1)
                + "\" customWidth=\"1\"/>\n";
        }
        xml += "  </cols>\n";
    }
    xml += "  <sheetData>\n";

    // Headers
    if (!sheet.headers.isEmpty()) {
        QByteArray rowXml;
        for (int c = 0; c < sheet.headers.size(); ++c) {
            int idx = getOrCreateSharedStringIndex(sheet.headers[c]);
            rowXml += QByteArray("      <c r=\"") + toExcelColumn(c) + "1\" t=\"s\" s=\"1\">"
                      "<v>" + QByteArray::number(idx) + "</v></c>\n";
        }
        xml += "    <row r=\"1\">\n" + rowXml + "    </row>\n";
    }

    // Data rows
    for (int r = 0; r < sheet.rows.size(); ++r) {
        int rowNum = r + 2; // 1-indexed, header is row 1
        xml += QByteArray("    <row r=\"") + QByteArray::number(rowNum) + "\">\n";
        for (int c = 0; c < sheet.rows[r].size(); ++c) {
            const QVariant &val = sheet.rows[r][c];
            bool isNumeric = false;
            double numVal = 0;
            if (val.typeId() == QMetaType::Int || val.typeId() == QMetaType::Double) {
                isNumeric = true;
                numVal = val.toDouble();
            }
            if (isNumeric) {
                xml += QByteArray("      <c r=\"") + toExcelColumn(c) + QByteArray::number(rowNum)
                    + "\"><v>" + QByteArray::number(numVal) + "</v></c>\n";
            } else {
                int idx = getOrCreateSharedStringIndex(val.toString());
                xml += QByteArray("      <c r=\"") + toExcelColumn(c) + QByteArray::number(rowNum)
                    + "\" t=\"s\"><v>" + QByteArray::number(idx) + "</v></c>\n";
            }
        }
        xml += "    </row>\n";
    }

    xml += "  </sheetData>\n</worksheet>";
    return xml;
}

bool XlsxWriter::save(const QString &path)
{
    mz_zip_archive zip;
    memset(&zip, 0, sizeof(zip));
    if (!mz_zip_writer_init_file(&zip, path.toUtf8().constData(), 0)) {
        qWarning() << "Failed to create ZIP file:" << path;
        return false;
    }

    auto addFile = [&](const char *name, const QByteArray &content) -> bool {
        return mz_zip_writer_add_mem(&zip, name, content.constData(), content.size(), MZ_DEFAULT_COMPRESSION) != 0;
    };

    auto addFiles = {addFile("[Content_Types].xml", buildContentTypes()),
                     addFile("_rels/.rels", buildRels()),
                     addFile("xl/workbook.xml", buildWorkbook()),
                     addFile("xl/_rels/workbook.xml.rels", buildWorkbookRels()),
                     addFile("xl/sharedStrings.xml", buildSharedStrings())};

    for (bool ok : addFiles) {
        if (!ok) { qWarning() << "Failed to add file to XLSX ZIP"; return false; }
    }

    for (int i = 0; i < m_sheets.size(); ++i) {
        QByteArray name = QByteArray("xl/worksheets/sheet") + QByteArray::number(i + 1) + ".xml";
        if (!addFile(name.constData(), buildSheet(m_sheets[i], i)))
            return false;
    }

    if (!mz_zip_writer_finalize_archive(&zip)) {
        qWarning() << "Failed to finalize XLSX ZIP";
        mz_zip_writer_end(&zip);
        return false;
    }
    mz_zip_writer_end(&zip);
    return true;
}
