#pragma once

#include <QColor>
#include <QIcon>
#include <QString>

namespace IconUtils {

QIcon tinted(const QString &resourcePath, const QColor &color, int size = 24);

} // namespace IconUtils
