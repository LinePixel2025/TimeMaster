#include "app_icon_provider.h"

#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QFileInfo>
#include <QDir>

#include <windows.h>
#include <shellapi.h>

AppIconProvider* AppIconProvider::instance()
{
    static AppIconProvider inst;
    return &inst;
}

AppIconProvider::AppIconProvider()
{
    createFallbackIcon();
}

void AppIconProvider::createFallbackIcon()
{
    QPixmap pix(24, 24);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);

    QPainterPath path;
    path.addRoundedRect(2, 2, 20, 20, 4, 4);
    p.setPen(QPen(QColor("#9CA3AF"), 1.5));
    p.setBrush(QColor("#E5E7EB"));
    p.drawPath(path);

    p.setPen(QPen(QColor("#9CA3AF"), 1.2));
    p.drawLine(6, 9, 18, 9);
    p.drawLine(6, 13, 15, 13);
    p.drawLine(6, 17, 12, 17);

    p.end();
    m_fallbackIcon = QIcon(pix);
}

QIcon AppIconProvider::icon(const QString &processPath, int size)
{
    QMutexLocker lock(&m_mutex);

    if (processPath.isEmpty())
        return m_fallbackIcon;

    QString cacheKey = processPath + "_" + QString::number(size);
    if (m_cache.contains(cacheKey))
        return m_cache[cacheKey];

    QIcon result = extractIcon(processPath, size);
    m_cache[cacheKey] = result;
    return result;
}

QIcon AppIconProvider::extractIcon(const QString &processPath, int size)
{
    QFileInfo fi(processPath);
    if (!fi.exists())
        return m_fallbackIcon;

    QString nativePath = QDir::toNativeSeparators(processPath);

    SHFILEINFOW sfi = {};
    DWORD_PTR result = SHGetFileInfoW(
        reinterpret_cast<LPCWSTR>(nativePath.utf16()),
        0, &sfi, sizeof(sfi),
        SHGFI_ICON | SHGFI_LARGEICON);

    if (result == 0)
        return m_fallbackIcon;

    HICON hIcon = sfi.hIcon;
    if (!hIcon)
        return m_fallbackIcon;

    ICONINFO iconInfo = {};
    if (!GetIconInfo(hIcon, &iconInfo)) {
        DestroyIcon(hIcon);
        return m_fallbackIcon;
    }

    QPixmap pix;
    if (iconInfo.hbmColor) {
        BITMAP bm = {};
        GetObject(iconInfo.hbmColor, sizeof(bm), &bm);

        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = bm.bmWidth;
        bmi.bmiHeader.biHeight = -bm.bmHeight;
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        QImage image(bm.bmWidth, bm.bmHeight, QImage::Format_ARGB32);
        HDC hdcScreen = GetDC(nullptr);
        GetDIBits(hdcScreen, iconInfo.hbmColor, 0, bm.bmHeight,
                  image.bits(), &bmi, DIB_RGB_COLORS);
        ReleaseDC(nullptr, hdcScreen);

        if (bm.bmWidth != size || bm.bmHeight != size)
            image = image.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);

        pix = QPixmap::fromImage(image);
    }

    if (iconInfo.hbmColor)
        DeleteObject(iconInfo.hbmColor);
    if (iconInfo.hbmMask)
        DeleteObject(iconInfo.hbmMask);
    DestroyIcon(hIcon);

    if (pix.isNull())
        return m_fallbackIcon;

    return QIcon(pix);
}
