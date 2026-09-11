#include "SettingsDialog.h"

#include "Settings.h"
#include "Theme.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFontDatabase>
#include <QFormLayout>
#include <QHeaderView>
#include <QKeySequence>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSet>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QPlainTextEdit>
#include <QSignalBlocker>
#include <QMap>

SettingsDialog::SettingsDialog(IdeSettings *settings, QWidget *parent)
    : QDialog(parent), settings_(settings) {
    setWindowTitle("Settings");
    resize(720, 560);

    auto *root = new QVBoxLayout(this);
    auto *tabs = new QTabWidget;
    root->addWidget(tabs, 1);

    // --- Appearance -------------------------------------------------------
    auto *appearance = new QWidget;
    auto *appearanceForm = new QFormLayout(appearance);
    theme_ = new QComboBox;
    theme_->addItems(Theme::names());
    theme_->setCurrentText(settings_->theme());

    monospaceOnly_ = new QCheckBox("Only list fixed-width fonts");
    monospaceOnly_->setChecked(true);
    font_ = new QComboBox;
    populateFontFamilies(true);
    font_->setCurrentText(settings_->fontFamily());

    fontSize_ = new QSpinBox;
    fontSize_->setRange(8, 32);
    fontSize_->setSuffix(" pt");
    fontSize_->setValue(settings_->fontSize());
    terminalFontSize_ = new QSpinBox;
    terminalFontSize_->setRange(8, 32);
    terminalFontSize_->setSuffix(" pt");
    terminalFontSize_->setValue(settings_->terminalFontSize());

    preview_ = new QPlainTextEdit;
    preview_->setReadOnly(true);
    preview_->setFixedHeight(96);
    preview_->setPlainText("bring std.io\n\nlaunch:\n    say \"hello from PunPun\"\ndone");

    appearanceForm->addRow("Theme", theme_);
    appearanceForm->addRow("Editor font", font_);
    appearanceForm->addRow(QString(), monospaceOnly_);
    appearanceForm->addRow("Editor font size", fontSize_);
    appearanceForm->addRow("Terminal font size", terminalFontSize_);
    appearanceForm->addRow("Preview", preview_);
    tabs->addTab(appearance, "Appearance");

    connect(monospaceOnly_, &QCheckBox::toggled, this, [this](bool on) {
        const QString current = font_->currentText();
        populateFontFamilies(on);
        font_->setCurrentText(current);
        refreshPreview();
    });
    connect(font_, &QComboBox::currentTextChanged, this, [this] { refreshPreview(); });
    connect(fontSize_, QOverload<int>::of(&QSpinBox::valueChanged), this, [this] { refreshPreview(); });
    refreshPreview();

    // --- Editor -----------------------------------------------------------
    auto *editor = new QWidget;
    auto *editorForm = new QFormLayout(editor);
    tabWidth_ = new QSpinBox;
    tabWidth_->setRange(2, 8);
    tabWidth_->setSuffix(" spaces");
    tabWidth_->setValue(settings_->tabWidth());
    wrap_ = new QCheckBox("Wrap long lines at the window edge");
    wrap_->setChecked(settings_->wordWrap());
    autoIndent_ = new QCheckBox("Keep indentation on Enter, and indent after a block opens");
    autoIndent_->setChecked(settings_->autoIndent());
    autoClose_ = new QCheckBox("Close brackets and quotes as you type");
    autoClose_->setChecked(settings_->autoCloseBrackets());
    highlightLine_ = new QCheckBox("Highlight the line holding the caret");
    highlightLine_->setChecked(settings_->highlightCurrentLine());
    showWhitespace_ = new QCheckBox("Show tabs and spaces");
    showWhitespace_->setChecked(settings_->showWhitespace());

    editorForm->addRow("Tab width", tabWidth_);
    editorForm->addRow(QString(), wrap_);
    editorForm->addRow(QString(), autoIndent_);
    editorForm->addRow(QString(), autoClose_);
    editorForm->addRow(QString(), highlightLine_);
    editorForm->addRow(QString(), showWhitespace_);
    tabs->addTab(editor, "Editor");

    // --- Files ------------------------------------------------------------
    auto *files = new QWidget;
    auto *filesForm = new QFormLayout(files);
    autosave_ = new QCheckBox("Save a file automatically after it changes");
    autosave_->setChecked(settings_->autoSave());
    trimWhitespace_ = new QCheckBox("Remove trailing whitespace when saving");
    trimWhitespace_->setChecked(settings_->trimTrailingWhitespace());
    finalNewline_ = new QCheckBox("End every saved file with a newline");
    finalNewline_->setChecked(settings_->insertFinalNewline());
    restoreProject_ = new QCheckBox("Reopen the last folder on start");
    restoreProject_->setChecked(settings_->restoreLastProject());
    filesForm->addRow(QString(), autosave_);
    filesForm->addRow(QString(), trimWhitespace_);
    filesForm->addRow(QString(), finalNewline_);
    filesForm->addRow(QString(), restoreProject_);
    tabs->addTab(files, "Files");

    // --- Updates ----------------------------------------------------------
    auto *updates = new QWidget;
    auto *updateForm = new QFormLayout(updates);
    autoupdate_ = new QCheckBox("Automatically install the latest stable PunPun toolchain");
    autoupdate_->setChecked(settings_->autoUpdatePunPun());
    interval_ = new QSpinBox;
    interval_->setRange(5, 1440);
    interval_->setSuffix(" min");
    interval_->setValue(settings_->updateIntervalMinutes());
    auto *updateNote = new QLabel(
        "Update checks contact the public PunPun release feed on GitHub. When a check "
        "cannot reach the network the IDE stays quiet and retries later.");
    updateNote->setWordWrap(true);
    updateForm->addRow(autoupdate_);
    updateForm->addRow("Check every", interval_);
    updateForm->addRow(updateNote);
    tabs->addTab(updates, "PunPun Updates");

    // --- Keybindings ------------------------------------------------------
    auto *keyPage = new QWidget;
    auto *keyLayout = new QVBoxLayout(keyPage);
    auto *keyHelp = new QLabel(
        "Click a keybinding to edit it. Empty disables a shortcut. Duplicate shortcuts are rejected.");
    keyHelp->setWordWrap(true);
    keyLayout->addWidget(keyHelp);

    keys_ = new QTableWidget;
    keys_->setColumnCount(2);
    keys_->setHorizontalHeaderLabels({"Command", "Keybinding"});
    keys_->horizontalHeader()->setStretchLastSection(true);
    keys_->verticalHeader()->setVisible(false);
    keys_->setAlternatingRowColors(true);

    // Readable names for the stored command ids.
    static const QMap<QString, QString> labels{
        {"file.new", "New file"},              {"file.open", "Open file"},
        {"file.openFolder", "Open folder"},    {"file.save", "Save"},
        {"file.saveAll", "Save all"},          {"run.run", "Run"},
        {"run.debug", "Debug"},                {"run.check", "Check"},
        {"view.terminal", "Toggle terminal"},  {"view.assistant", "Toggle assistant"},
        {"editor.complete", "Trigger completion"}, {"editor.find", "Find"},
        {"editor.format", "Format document"},
    };

    const auto bindings = settings_->keybindings();
    keys_->setRowCount(bindings.size());
    int row = 0;
    for (auto it = bindings.cbegin(); it != bindings.cend(); ++it, ++row) {
        auto *id = new QTableWidgetItem(labels.value(it.key(), it.key()));
        id->setFlags(id->flags() & ~Qt::ItemIsEditable);
        id->setData(Qt::UserRole, it.key());
        id->setToolTip(it.key());
        keys_->setItem(row, 0, id);
        keys_->setItem(row, 1, new QTableWidgetItem(it.value()));
    }
    keys_->sortItems(0);
    keyLayout->addWidget(keys_, 1);
    tabs->addTab(keyPage, "Keybindings");

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply);
    root->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        if (save()) accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons->button(QDialogButtonBox::Apply), &QPushButton::clicked,
            this, [this] { save(); });
}

void SettingsDialog::populateFontFamilies(bool monospaceOnly) {
    QSignalBlocker blocker(font_);
    font_->clear();
    QStringList families;
    for (const QString &family : QFontDatabase::families()) {
        if (monospaceOnly && !QFontDatabase::isFixedPitch(family)) continue;
        families << family;
    }
    // Never present an empty list; fall back to everything if the host reports
    // no fixed-pitch families.
    if (families.isEmpty()) families = QFontDatabase::families();
    font_->addItems(families);
}

void SettingsDialog::refreshPreview() {
    QFont font(font_->currentText());
    font.setStyleHint(QFont::Monospace);
    font.setPointSize(fontSize_->value());
    preview_->setFont(font);
}

bool SettingsDialog::save() {
    QSet<QString> used;
    for (int row = 0; row < keys_->rowCount(); ++row) {
        const QString raw = keys_->item(row, 1)->text().trimmed();
        if (raw.isEmpty()) continue;

        const QKeySequence sequence(raw);
        const QString canonical = sequence.toString(QKeySequence::PortableText);
        if (canonical.isEmpty()) {
            QMessageBox::warning(this, "Invalid keybinding",
                                 QString("%1 is not a valid key sequence.").arg(raw));
            keys_->setCurrentCell(row, 1);
            return false;
        }
        if (used.contains(canonical)) {
            QMessageBox::warning(this, "Duplicate keybinding",
                                 QString("%1 is assigned more than once.").arg(canonical));
            keys_->setCurrentCell(row, 1);
            return false;
        }
        used.insert(canonical);
        keys_->item(row, 1)->setText(canonical);
    }

    settings_->setTheme(theme_->currentText());
    settings_->setFontFamily(font_->currentText());
    settings_->setFontSize(fontSize_->value());
    settings_->setTabWidth(tabWidth_->value());
    settings_->setWordWrap(wrap_->isChecked());
    settings_->setAutoSave(autosave_->isChecked());
    settings_->setAutoIndent(autoIndent_->isChecked());
    settings_->setAutoCloseBrackets(autoClose_->isChecked());
    settings_->setHighlightCurrentLine(highlightLine_->isChecked());
    settings_->setShowWhitespace(showWhitespace_->isChecked());
    settings_->setTrimTrailingWhitespace(trimWhitespace_->isChecked());
    settings_->setInsertFinalNewline(finalNewline_->isChecked());
    settings_->setRestoreLastProject(restoreProject_->isChecked());
    settings_->setTerminalFontSize(terminalFontSize_->value());
    settings_->setAutoUpdatePunPun(autoupdate_->isChecked());
    settings_->setUpdateIntervalMinutes(interval_->value());
    for (int row = 0; row < keys_->rowCount(); ++row) {
        // Column 0 shows a friendly label; the stored id rides in UserRole.
        const QString id = keys_->item(row, 0)->data(Qt::UserRole).toString();
        settings_->setKeybinding(id.isEmpty() ? keys_->item(row, 0)->text() : id,
                                 keys_->item(row, 1)->text().trimmed());
    }
    Q_EMIT settingsChanged();
    return true;
}
