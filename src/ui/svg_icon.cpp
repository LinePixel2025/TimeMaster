#include "ui/svg_icon.h"
#include <QSvgRenderer>
#include <QPainter>
#include <QPixmap>

QIcon makeSvgIcon(const QString &resourcePath)
{
    QSvgRenderer renderer(resourcePath);
    if (!renderer.isValid())
        return QIcon();

    QPixmap pixmap(24, 24);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    renderer.render(&painter);
    painter.end();

    return QIcon(pixmap);
}
