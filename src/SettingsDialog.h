#pragma once

#include <QDialog>

class IdeSettings;
class QCheckBox;
class QComboBox;
class QLineEdit;
class QSpinBox;
class QTableWidget;
class QPlainTextEdit;

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(IdeSettings *settings, QWidget *parent = nullptr);

Q_SIGNALS:
    void settingsChanged();

private:
    IdeSettings *settings_ = nullptr;
    QComboBox *theme_ = nullptr;
    QComboBox *font_ = nullptr;
    QSpinBox *fontSize_ = nullptr;
    QSpinBox *tabWidth_ = nullptr;
    QSpinBox *interval_ = nullptr;
    QLineEdit *toolchainDir_ = nullptr;
    QSpinBox *terminalFontSize_ = nullptr;
    QCheckBox *wrap_ = nullptr;
    QCheckBox *autosave_ = nullptr;
    QCheckBox *autoupdate_ = nullptr;
    QCheckBox *monospaceOnly_ = nullptr;
    QCheckBox *autoIndent_ = nullptr;
    QCheckBox *autoClose_ = nullptr;
    QCheckBox *highlightLine_ = nullptr;
    QCheckBox *showWhitespace_ = nullptr;
    QCheckBox *trimWhitespace_ = nullptr;
    QCheckBox *finalNewline_ = nullptr;
    QCheckBox *restoreProject_ = nullptr;
    QPlainTextEdit *preview_ = nullptr;
    QTableWidget *keys_ = nullptr;

    void populateFontFamilies(bool monospaceOnly);
    void refreshPreview();
    bool save();
};
