#pragma once

#include <QMap>
#include <QSettings>
#include <QString>

class IdeSettings {
public:
    IdeSettings();

    QString theme() const;
    void setTheme(const QString &value);

    QString fontFamily() const;
    void setFontFamily(const QString &value);

    int fontSize() const;
    void setFontSize(int value);

    int tabWidth() const;
    void setTabWidth(int value);

    bool wordWrap() const;
    void setWordWrap(bool value);

    bool autoIndent() const;
    void setAutoIndent(bool value);

    bool autoCloseBrackets() const;
    void setAutoCloseBrackets(bool value);

    bool highlightCurrentLine() const;
    void setHighlightCurrentLine(bool value);

    bool showWhitespace() const;
    void setShowWhitespace(bool value);

    bool trimTrailingWhitespace() const;
    void setTrimTrailingWhitespace(bool value);

    bool insertFinalNewline() const;
    void setInsertFinalNewline(bool value);

    int terminalFontSize() const;
    void setTerminalFontSize(int value);

    bool restoreLastProject() const;
    void setRestoreLastProject(bool value);

    bool autoSave() const;
    void setAutoSave(bool value);

    bool autoUpdatePunPun() const;
    void setAutoUpdatePunPun(bool value);

    int updateIntervalMinutes() const;
    void setUpdateIntervalMinutes(int value);

    QString lastProject() const;
    void setLastProject(const QString &value);

    /// Directory holding `ppc`/`pp`, when the user has one the IDE cannot
    /// discover on its own. Empty means "search the usual places".
    QString toolchainDir() const;
    void setToolchainDir(const QString &value);

    QMap<QString, QString> keybindings() const;
    void setKeybinding(const QString &action, const QString &sequence);

private:
    mutable QSettings s_;
};
