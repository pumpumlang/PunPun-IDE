#pragma once

#include <QCompleter>
#include <QEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPlainTextEdit>
#include <QPoint>
#include <QRect>
#include <QResizeEvent>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVector>
#include <QWidget>

#include "punpun_plugin_api.h"

class ScriptHost;
class SyntaxHighlighter;
class LineNumberArea;

struct EditorDiagnostic {
    int line = 0;
    int column = 0;
    int length = 1;
    int severity = 1; // 1 error, 2 warning, 3 information/hint
    QString message;
    QString code;
    QString source;
    QString suggestion;
};

class CodeEditor : public QPlainTextEdit {
    Q_OBJECT
public:
    explicit CodeEditor(QWidget *parent = nullptr);

    void setFilePath(const QString &path);
    QString filePath() const { return filePath_; }
    ppide_language language() const { return language_; }

    void configure(int fontSize, const QString &fontFamily, int tabWidth,
                   bool wordWrap, ScriptHost *scripts);
    void applyDiagnostics(const QVector<EditorDiagnostic> &items);
    const QVector<EditorDiagnostic> &diagnostics() const { return diagnostics_; }
    void setCompletionItems(const QStringList &items);
    /// Symbols the compiler reports for this file. Merged with the built-in
    /// words, never substituted for them.
    void setLanguageSymbols(const QStringList &symbols);
    QString currentWord() const;
    QPoint cursorGlobalBottom() const;
    void showCompletion();
    void gotoLine(int line, int column = 0);

    int lineNumberAreaWidth() const;
    void lineNumberAreaPaintEvent(QPaintEvent *event);

    // Editing behaviour, all individually switchable from Settings.
    void setEditingBehaviour(bool autoIndent, bool autoCloseBrackets,
                             bool highlightCurrentLine, bool showWhitespace,
                             bool trimTrailingWhitespaceOnSave, bool insertFinalNewline);
    bool trimTrailingWhitespaceOnSave() const { return trimOnSave_; }
    bool insertFinalNewline() const { return finalNewline_; }
    void setEditorPalette(const QColor &lineHighlight, const QColor &selection,
                          const QColor &muted, const QColor &accent);

    /// Normalise the buffer just before saving (trailing space, final newline).
    void prepareForSave();

    // Commands surfaced in the Edit menu and bound to shortcuts.
    void toggleLineComment();
    void indentSelection();
    void dedentSelection();
    void duplicateSelection();
    void moveSelectedLines(int delta);

Q_SIGNALS:
    void cursorLocationChanged(int line, int column);
    void hoverRequested(const QString &path, int line, int column,
                        const QString &word, QPoint globalPos);
    void hoverDismissRequested();
    void contentChangedForLanguageService(const QString &path, const QString &text);

protected:
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private Q_SLOTS:
    void updateLineNumberAreaWidth(int);
    void updateLineNumberArea(const QRect &, int);
    void insertCompletion(const QString &);
    void emitCursorPosition();
    void refreshDecorations();

private:
    QString indentUnit() const;
    QString lineCommentToken() const;
    bool handleNewline();
    bool handleCloseCharacter(QKeyEvent *event);
    bool handleBackspacePair();
    /// Indent/dedent every line the selection touches, keeping it selected.
    void shiftLines(bool forward);

    QString filePath_;
    ppide_language language_ = PPIDE_LANG_TEXT;
    LineNumberArea *lineArea_ = nullptr;
    SyntaxHighlighter *highlighter_ = nullptr;
    QCompleter *completer_ = nullptr;
    QStringList completionItems_;
    QStringList baseCompletionItems_;
    QStringList languageSymbols_;

    void rebuildCompletionModel();
    QVector<EditorDiagnostic> diagnostics_;
    QTimer hoverTimer_;
    QPoint pendingHoverPos_;

    int tabWidth_ = 4;
    bool autoIndent_ = true;
    bool autoClose_ = true;
    bool highlightLine_ = true;
    bool trimOnSave_ = true;
    bool finalNewline_ = true;
    QColor lineHighlightColor_{"#202020"};
    QColor selectionColor_{"#264f78"};
    QColor mutedColor_{"#8b8b8b"};
    QColor accentColor_{"#3794ff"};
};

class LineNumberArea : public QWidget {
public:
    explicit LineNumberArea(CodeEditor *editor) : QWidget(editor), editor_(editor) {}
    QSize sizeHint() const override { return {editor_->lineNumberAreaWidth(), 0}; }
protected:
    void paintEvent(QPaintEvent *event) override { editor_->lineNumberAreaPaintEvent(event); }
private:
    CodeEditor *editor_;
};
