#include "IconUtils.h"

#include <QPainter>
#include <QPixmap>
#include <QSvgRenderer>

namespace IconUtils {

QIcon tinted(const QString &resourcePath, const QColor &color, int size) {
    QSvgRenderer renderer(resourcePath);
    if (!renderer.isValid()) {
        return QIcon(resourcePath);
    }

    QPixmap source(size, size);
    source.fill(Qt::transparent);
    {
        QPainter painter(&source);
        renderer.render(&painter);
    }

    QPixmap tintedPixmap(size, size);
    tintedPixmap.fill(Qt::transparent);
    {
        QPainter painter(&tintedPixmap);
        painter.drawPixmap(0, 0, source);
        painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
        painter.fillRect(tintedPixmap.rect(), color);
    }

    return QIcon(tintedPixmap);
}

} // namespace IconUtils
