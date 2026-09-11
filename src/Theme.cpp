#include "Theme.h"

#include <QPair>
#include <QVector>
#include <algorithm>

namespace {
ThemePalette darkModern() {
    return {"#181818", "#181818", "#1f1f1f", "#1e1e1e", "#2b2b2b", "#cccccc", "#8b8b8b",
            "#3794ff", "#4ec9b0", "#89d185", "#cca700", "#f14c4c", "#264f78", "#202020"};
}

ThemePalette midnight() {
    return {"#0d1117", "#0d1117", "#161b22", "#0d1117", "#252b33", "#d8dee9", "#7d8590",
            "#58a6ff", "#4ec9b0", "#89d185", "#d29922", "#f85149", "#1f3a5f", "#121820"};
}

ThemePalette mocha() {
    return {"#11111b", "#181825", "#1e1e2e", "#11111b", "#313244", "#cdd6f4", "#7f849c",
            "#89b4fa", "#94e2d5", "#a6e3a1", "#f9e2af", "#f38ba8", "#313244", "#181825"};
}

ThemePalette graphite() {
    return {"#17191d", "#1b1e23", "#22262c", "#191c21", "#323740", "#d7dce2", "#858c98",
            "#7aa2f7", "#73daca", "#9ece6a", "#e0af68", "#f7768e", "#29354a", "#1e2228"};
}

ThemePalette light() {
    return {"#f7f7f7", "#f3f3f3", "#ffffff", "#ffffff", "#d4d4d4", "#333333", "#737373",
            "#005fb8", "#16825d", "#107c10", "#8a6d00", "#c42b1c", "#add6ff", "#f2f2f2"};
}

QString color(const QColor &value) { return value.name(QColor::HexRgb); }

// Substitute @name tokens rather than positional %1..%N placeholders. The
// positional form silently mismatched: the sheet used nine markers while
// fourteen values were chained onto it, so `.arg()` filled the highest marker
// with the ninth colour and dropped the rest.
QString fill(QString sheet, const ThemePalette &p) {
    const QVector<QPair<QString, QColor>> tokens{
        {"@window", p.window},     {"@panel", p.panel},       {"@panelAlt", p.panelAlt},
        {"@editor", p.editor},     {"@border", p.border},     {"@text", p.text},
        {"@muted", p.muted},       {"@accent", p.accent},     {"@accent2", p.accent2},
        {"@green", p.green},       {"@yellow", p.yellow},     {"@red", p.red},
        {"@selection", p.selection}, {"@lineHighlight", p.lineHighlight},
    };
    // Longest first so "@panelAlt" is never clipped by "@panel".
    QVector<QPair<QString, QColor>> ordered = tokens;
    std::sort(ordered.begin(), ordered.end(),
              [](const auto &a, const auto &b) { return a.first.size() > b.first.size(); });
    for (const auto &token : ordered) sheet.replace(token.first, color(token.second));
    return sheet;
}
}

QStringList Theme::names() {
    return {"Dark Modern", "Midnight", "Catppuccin Mocha", "Graphite", "Light"};
}

ThemePalette Theme::palette(const QString &name) {
    if (name == "Midnight") return midnight();
    if (name == "Catppuccin Mocha") return mocha();
    if (name == "Graphite") return graphite();
    if (name == "Light") return light();
    return darkModern();
}

QString Theme::stylesheet(const QString &name) {
    const auto p = palette(name);
    QString sheet = QString::fromUtf8(R"CSS(
        * { font-family: "Inter", "Segoe UI", sans-serif; font-size: 13px; }
        QMainWindow, QWidget#root { background:@window; color:@text; }
        QMenuBar { background:@panel; color:@text; border-bottom:1px solid @border; padding:1px 4px; }
        QMenuBar::item { padding:5px 8px; border-radius:3px; }
        QMenuBar::item:selected, QMenu::item:selected { background:@panelAlt; }
        QMenu { background:@panelAlt; color:@text; border:1px solid @border; padding:5px; }
        QMenu::item { padding:5px 24px 5px 8px; border-radius:3px; }
        QToolTip { background:@panelAlt; color:@text; border:1px solid @border; padding:6px 8px; }
        QAbstractItemView#completionPopup { background:@panelAlt; color:@text; border:1px solid @border; outline:0; padding:3px; }
        QAbstractItemView#completionPopup::item { min-height:24px; padding:2px 8px; border-radius:3px; }
        QAbstractItemView#completionPopup::item:selected { background:@selection; color:@text; }

        QFrame#activityBar { background:@panel; border-right:1px solid @border; }
        QToolButton#activityButton { border:0; border-left:2px solid transparent; border-radius:0; padding:10px; margin:0; }
        QToolButton#activityButton:hover { background:@panelAlt; }
        QToolButton#activityButton:checked { background:@panelAlt; border-left:2px solid @accent; }

        QFrame#sideBar, QFrame#bottomPanel, QStackedWidget#sideBarStack { background:@panel; }
        QWidget#editorRegion { background:@editor; }
        QFrame#editorToolbar { background:@panel; border-bottom:1px solid @border; }
        QLabel#sectionTitle { color:@muted; font-weight:700; letter-spacing:1px; font-size:11px; padding:8px 10px; }
        QLabel#brandText { font-size:21px; font-weight:650; }
        QLabel#muted { color:@muted; }

        QTreeView, QListWidget, QPlainTextEdit, QTextBrowser {
            background:@editor; color:@text; border:0;
            selection-background-color:@selection; selection-color:@text;
        }
        QTreeView { background:@panel; }
        QTreeView::item { height:25px; padding-left:3px; }
        QTreeView::item:hover, QListWidget::item:hover { background:@panelAlt; }
        QTreeView::item:selected, QListWidget::item:selected { background:@selection; }

        QLineEdit, QComboBox, QSpinBox {
            border:1px solid @border; border-radius:4px; padding:6px 8px; background:@editor; color:@text;
            selection-background-color:@selection;
        }
        QLineEdit:focus, QComboBox:focus, QSpinBox:focus { border-color:@accent; }
        QPushButton { background:@panelAlt; color:@text; border:1px solid @border; border-radius:4px; padding:6px 12px; }
        QPushButton:hover { background:@editor; border-color:@muted; }
        QPushButton:pressed { background:@selection; }
        QPushButton:disabled { color:@muted; background:@panel; border-color:@border; }
        QPushButton:focus { border-color:@accent; }
        QPushButton#primary { background:@accent; color:#ffffff; border-color:@accent; font-weight:600; }
        QPushButton#primary:hover { background:@accent2; border-color:@accent2; color:@window; }
        QPushButton#primary:pressed { background:@accent; }
        QPushButton#compactButton { padding:4px 9px; }

        QTabWidget::pane { border:0; background:@editor; }
        QTabBar { background:@panel; }
        QTabBar::tab { background:@panel; color:@muted; padding:8px 14px; border-right:1px solid @border; border-top:1px solid transparent; }
        QTabBar::tab:hover { background:@panelAlt; color:@text; }
        QTabBar::tab:selected { background:@editor; color:@text; border-top:1px solid @accent; }
        QTabBar::close-button { subcontrol-position:right; }
        QToolButton#tabCloseButton { border:0; border-radius:3px; margin-left:4px; background:transparent; }
        QToolButton#tabCloseButton:hover { background:@selection; }

        QSplitter::handle { background:@border; width:1px; height:1px; }
        QSplitter::handle:hover { background:@accent; }
        QTabBar::tab:!selected { border-top:1px solid transparent; }
        QCheckBox { padding:2px 0; }
        QCheckBox::indicator { width:15px; height:15px; border:1px solid @border; border-radius:3px; background:@editor; }
        QCheckBox::indicator:checked { background:@accent; border-color:@accent; }
        QCheckBox::indicator:hover { border-color:@accent; }
        QScrollBar:vertical { width:11px; background:transparent; margin:0; }
        QScrollBar::handle:vertical { background:@border; min-height:32px; border-radius:5px; margin:2px; }
        QScrollBar::handle:vertical:hover { background:@muted; }
        QScrollBar:horizontal { height:11px; background:transparent; margin:0; }
        QScrollBar::handle:horizontal { background:@border; min-width:32px; border-radius:5px; margin:2px; }
        QScrollBar::handle:horizontal:hover { background:@muted; }
        QScrollBar::add-line, QScrollBar::sub-line { width:0; height:0; }
        QScrollBar::add-page, QScrollBar::sub-page { background:transparent; }

        QStatusBar { background:@panel; color:@muted; border-top:1px solid @border; }
        QStatusBar QLabel { color:@muted; padding:1px 7px; }

        QWidget#assistantPanel { background:@panel; border-left:1px solid @border; }
        QWidget#assistantHeader { background:@panel; border-bottom:1px solid @border; }
        QLabel#assistantSummary { color:@text; background:@panelAlt; border-bottom:1px solid @border; }
        QWidget#assistantCounts { background:@panel; border-bottom:1px solid @border; }
        QLabel#assistantErrors { color:@red; font-size:11px; font-weight:600; }
        QLabel#assistantWarnings { color:@yellow; font-size:11px; font-weight:600; }
        QLabel#assistantHints { color:@accent; font-size:11px; font-weight:600; }
        QListWidget#assistantList { background:@panel; outline:0; }
        QListWidget#assistantList::item { padding:7px 9px; border-bottom:1px solid @border; }
        QTextBrowser#assistantDetails { background:@panelAlt; border-top:1px solid @border; border-bottom:1px solid @border; padding:8px; }
        QLabel#assistantEnvironment { color:@muted; background:@panel; }

        QWidget#terminalWidget { background:@editor; }
        QWidget#terminalToolbar { background:@panel; border-top:1px solid @border; border-bottom:1px solid @border; }
        QWidget#terminalInputRow { background:@editor; border-top:1px solid @border; }
        QLabel#terminalTitle { color:@text; font-weight:600; }
        QLabel#terminalState { color:@muted; font-size:10px; font-weight:700; padding:1px 5px; border:1px solid @border; border-radius:3px; }
        QLabel#terminalCwd { color:@muted; }
        QLabel#terminalPrompt { color:@accent; font-family:"JetBrains Mono", "Cascadia Code", monospace; font-weight:700; }
        QPlainTextEdit#terminalOutput { background:@editor; color:@text; font-family:"JetBrains Mono", "Cascadia Code", monospace; padding:7px 10px; selection-background-color:@selection; }
        QLineEdit#terminalInput { background:@editor; color:@text; border:0; border-radius:0; font-family:"JetBrains Mono", "Cascadia Code", monospace; padding:5px 2px; }

        QFrame#hoverCard { background:@panelAlt; border:1px solid @border; border-radius:5px; }
        QTextBrowser#hoverText { background:@panelAlt; color:@text; }
        QTextBrowser#markdownPreview { background:@editor; padding:18px; }

        QDialog { background:@window; color:@text; }
        QGroupBox { border:1px solid @border; border-radius:5px; margin-top:12px; padding-top:10px; }
        QGroupBox::title { subcontrol-origin:margin; left:8px; padding:0 4px; color:@muted; }
    )CSS");
    return fill(sheet, p);
}
