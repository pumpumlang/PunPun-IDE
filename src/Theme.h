#pragma once
#include <QColor>
#include <QString>
#include <QStringList>

struct ThemePalette {
    QColor window, panel, panelAlt, editor, border, text, muted;
    QColor accent, accent2, green, yellow, red, selection, lineHighlight;
};

namespace Theme {
ThemePalette palette(const QString &name);
QString stylesheet(const QString &name);
QStringList names();
}
