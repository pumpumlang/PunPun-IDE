#include "FileIconProvider.h"
#include "IconUtils.h"

#include <QColor>
#include <QFileInfo>
#include <QIcon>

QIcon FileIconProvider::icon(const QFileInfo &info) const {
    if (info.isDir()) {
        return IconUtils::tinted(":/fluent/folder.svg", QColor("#d7ba7d"), 22);
    }

    const QString suffix = info.suffix().toLower();
    if (suffix == "pp") {
        return QIcon(":/branding/punpun-mark.svg");
    }
    if (suffix == "c") {
        return QIcon(":/file-icons/c.svg");
    }
    if (suffix == "cpp" || suffix == "cc" || suffix == "cxx") {
        return QIcon(":/file-icons/cpp.svg");
    }
    if (suffix == "h" || suffix == "hpp" || suffix == "hh" || suffix == "hxx") {
        return QIcon(":/file-icons/h.svg");
    }
    if (suffix == "md" || suffix == "markdown") {
        return QIcon(":/file-icons/markdown.svg");
    }
    if (suffix == "json") {
        return QIcon(":/file-icons/json.svg");
    }
    if (suffix == "js" || suffix == "mjs" || suffix == "cjs") {
        return QIcon(":/file-icons/javascript.svg");
    }
    if (suffix == "py") {
        return QIcon(":/file-icons/python.svg");
    }

    return QFileIconProvider::icon(info);
}
