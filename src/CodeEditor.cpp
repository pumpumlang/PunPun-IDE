#include "CodeEditor.h"

#include "PunPunLanguage.h"
#include "ScriptHost.h"
#include "SyntaxHighlighter.h"

#include <QAbstractItemView>
#include <QColor>
#include <QEvent>
#include <QFont>
#include <QFontDatabase>
#include <QFontMetricsF>
#include <QFrame>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QStringListModel>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextEdit>
#include <QTextFormat>
#include <QTextOption>
#include <climits>

CodeEditor::CodeEditor(QWidget *parent) : QPlainTextEdit(parent) {
    lineArea_ = new LineNumberArea(this);

    connect(this, &QPlainTextEdit::blockCountChanged,
            this, &CodeEditor::updateLineNumberAreaWidth);
    connect(this, &QPlainTextEdit::updateRequest,
            this, &CodeEditor::updateLineNumberArea);
    connect(this, &QPlainTextEdit::cursorPositionChanged,
            this, &CodeEditor::emitCursorPosition);
    connect(this, &QPlainTextEdit::cursorPositionChanged,
            this, &CodeEditor::refreshDecorations);
    connect(this, &QPlainTextEdit::selectionChanged, this, [this] {
        if (textCursor().hasSelection()) {
            hoverTimer_.stop();
            Q_EMIT hoverDismissRequested();
        }
    });
    connect(this, &QPlainTextEdit::textChanged, this, [this] {
        if (!filePath_.isEmpty())
            Q_EMIT contentChangedForLanguageService(filePath_, toPlainText());
    });

    updateLineNumberAreaWidth(0);
    setMouseTracking(true);
    setCenterOnScroll(true);
    setFrameStyle(QFrame::NoFrame);

    completer_ = new QCompleter(this);
    completer_->setWidget(this);
    completer_->setCompletionMode(QCompleter::PopupCompletion);
    completer_->setCaseSensitivity(Qt::CaseInsensitive);
    completer_->setWrapAround(false);
    completer_->popup()->setObjectName("completionPopup");
    connect(completer_, QOverload<const QString &>::of(&QCompleter::activated),
            this, &CodeEditor::insertCompletion);

    hoverTimer_.setSingleShot(true);
    hoverTimer_.setInterval(650);
    connect(&hoverTimer_, &QTimer::timeout, this, [this] {
        if (textCursor().hasSelection()) return;
        auto cursor = cursorForPosition(mapFromGlobal(pendingHoverPos_));
        cursor.select(QTextCursor::WordUnderCursor);
        const QString word = cursor.selectedText();
        if (!word.isEmpty()) {
            const int wordColumn = cursor.selectionStart() - cursor.block().position();
            Q_EMIT hoverRequested(filePath_, cursor.blockNumber(),
                                  qMax(0, wordColumn), word, pendingHoverPos_);
        }
    });
}

void CodeEditor::setFilePath(const QString &path) {
    filePath_ = path;
    language_ = ppide_detect_language(path.toUtf8().constData());
}

void CodeEditor::configure(int fontSize, const QString &family, int tabWidth,
                           bool wordWrap, ScriptHost *scripts) {
    QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    if (!family.isEmpty() && QFontDatabase::families().contains(family))
        font.setFamily(family);
    font.setPointSize(fontSize);
    setFont(font);

    tabWidth_ = qMax(2, tabWidth);
    setTabStopDistance(QFontMetricsF(font).horizontalAdvance(' ') * tabWidth_);
    setLineWrapMode(wordWrap ? QPlainTextEdit::WidgetWidth : QPlainTextEdit::NoWrap);

    QString key;
    switch (language_) {
        case PPIDE_LANG_PUNPUN: key = "punpun"; break;
        case PPIDE_LANG_C: key = "c"; break;
        case PPIDE_LANG_CPP: key = "cpp"; break;
        case PPIDE_LANG_HEADER: key = "header"; break;
        case PPIDE_LANG_MARKDOWN: key = "markdown"; break;
        case PPIDE_LANG_JAVASCRIPT: key = "javascript"; break;
        case PPIDE_LANG_PYTHON: key = "python"; break;
        case PPIDE_LANG_JSON: key = "json"; break;
        default: break;
    }

    delete highlighter_;
    highlighter_ = new SyntaxHighlighter(document(), language_,
                                         scripts ? scripts->keywords(key) : QStringList{});

    QStringList base = scripts ? scripts->keywords(key) : QStringList{};
    if (language_ == PPIDE_LANG_PUNPUN) {
        // Previously a hand-written list that offered `craft`, `pin`, `done`
        // and `bring` -- the migration dialect the compiler warns about -- and
        // `trait`/`impl`, which PunPun does not have. It now comes from the
        // compiler's own tables.
        base << PunPunLanguage::completionWords();
    }
    base.removeDuplicates();
    base.sort(Qt::CaseInsensitive);
    setCompletionItems(base);
    refreshDecorations();
}

void CodeEditor::setCompletionItems(const QStringList &items) {
    completionItems_ = items;
    auto *oldModel = completer_->model();
    completer_->setModel(new QStringListModel(items, completer_));
    if (oldModel && oldModel->parent() == completer_) oldModel->deleteLater();
}

QString CodeEditor::currentWord() const {
    auto cursor = textCursor();
    cursor.select(QTextCursor::WordUnderCursor);
    return cursor.selectedText();
}

QPoint CodeEditor::cursorGlobalBottom() const {
    return mapToGlobal(cursorRect().bottomLeft());
}

void CodeEditor::showCompletion() {
    const QString prefix = currentWord();
    completer_->setCompletionPrefix(prefix);
    if (completer_->completionCount() == 0) return;
    auto rect = cursorRect();
    rect.setWidth(qMax(300, completer_->popup()->sizeHintForColumn(0) + 36));
    completer_->complete(rect);
}

void CodeEditor::insertCompletion(const QString &text) {
    auto cursor = textCursor();
    cursor.select(QTextCursor::WordUnderCursor);
    cursor.insertText(text);
    setTextCursor(cursor);
}

void CodeEditor::applyDiagnostics(const QVector<EditorDiagnostic> &items) {
    diagnostics_ = items;
    refreshDecorations();
    lineArea_->update();
}

void CodeEditor::refreshDecorations() {
    QList<QTextEdit::ExtraSelection> selections;

    // A full-width current-line band, drawn first so selection and diagnostics
    // paint over it. Earlier builds dropped this because a too-bright colour
    // read as a floating box; a low-contrast tone from the active theme does
    // not, and losing the caret line entirely made the editor hard to track.
    if (highlightLine_ && !textCursor().hasSelection()) {
        QTextEdit::ExtraSelection line;
        line.format.setBackground(lineHighlightColor_);
        line.format.setProperty(QTextFormat::FullWidthSelection, true);
        line.cursor = textCursor();
        line.cursor.clearSelection();
        selections.push_back(line);
    }

    // Highlight the two matching delimiters near the caret.
    if (!textCursor().hasSelection()) {
        const QString text = toPlainText();
        int position = textCursor().position();
        int bracePosition = -1;
        if (position > 0 && QString("()[]{}").contains(text.at(position - 1))) bracePosition = position - 1;
        else if (position < text.size() && QString("()[]{}").contains(text.at(position))) bracePosition = position;

        if (bracePosition >= 0) {
            const QChar brace = text.at(bracePosition);
            QChar partner;
            int direction = 0;
            if (brace == '(') { partner = ')'; direction = 1; }
            else if (brace == '[') { partner = ']'; direction = 1; }
            else if (brace == '{') { partner = '}'; direction = 1; }
            else if (brace == ')') { partner = '('; direction = -1; }
            else if (brace == ']') { partner = '['; direction = -1; }
            else if (brace == '}') { partner = '{'; direction = -1; }

            int depth = 0;
            int match = -1;
            for (int i = bracePosition; i >= 0 && i < text.size(); i += direction) {
                const QChar current = text.at(i);
                if (current == brace) ++depth;
                else if (current == partner && --depth == 0) { match = i; break; }
            }
            for (int pos : {bracePosition, match}) {
                if (pos < 0) continue;
                QTextEdit::ExtraSelection pair;
                pair.cursor = QTextCursor(document());
                pair.cursor.setPosition(pos);
                pair.cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, 1);
                pair.format.setBackground(selectionColor_);
                pair.format.setForeground(accentColor_);
                pair.format.setFontWeight(QFont::Bold);
                selections.push_back(pair);
            }
        }
    }

    for (const auto &d : diagnostics_) {
        auto block = document()->findBlockByNumber(qMax(0, d.line));
        if (!block.isValid()) continue;
        QTextCursor cursor(block);
        cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor,
                            qMax(0, d.column));
        cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor,
                            qMax(1, d.length));
        QTextEdit::ExtraSelection selection;
        selection.cursor = cursor;
        QTextCharFormat format;
        format.setUnderlineStyle(QTextCharFormat::WaveUnderline);
        if (d.severity <= 1) format.setUnderlineColor(QColor("#f14c4c"));
        else if (d.severity == 2) format.setUnderlineColor(QColor("#cca700"));
        else format.setUnderlineColor(accentColor_);
        selection.format = format;
        selections.push_back(selection);
    }
    setExtraSelections(selections);
}

void CodeEditor::gotoLine(int line, int column) {
    auto block = document()->findBlockByNumber(qMax(0, line));
    if (!block.isValid()) return;
    QTextCursor cursor(block);
    cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, qMax(0, column));
    setTextCursor(cursor);
    centerCursor();
    setFocus();
}

int CodeEditor::lineNumberAreaWidth() const {
    int digits = 1;
    int maximum = qMax(1, blockCount());
    while (maximum >= 10) {
        maximum /= 10;
        ++digits;
    }
    return 22 + fontMetrics().horizontalAdvance('9') * digits;
}

void CodeEditor::updateLineNumberAreaWidth(int) {
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

void CodeEditor::updateLineNumberArea(const QRect &rect, int dy) {
    if (dy) lineArea_->scroll(0, dy);
    else lineArea_->update(0, rect.y(), lineArea_->width(), rect.height());
    if (rect.contains(viewport()->rect())) updateLineNumberAreaWidth(0);
}

void CodeEditor::resizeEvent(QResizeEvent *event) {
    QPlainTextEdit::resizeEvent(event);
    const auto contents = contentsRect();
    lineArea_->setGeometry(QRect(contents.left(), contents.top(),
                                 lineNumberAreaWidth(), contents.height()));
}

void CodeEditor::lineNumberAreaPaintEvent(QPaintEvent *event) {
    QPainter painter(lineArea_);
    painter.fillRect(event->rect(), QColor("#111419"));

    QTextBlock block = firstVisibleBlock();
    int number = block.blockNumber();
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + qRound(blockBoundingRect(block).height());

    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            painter.setPen(number == textCursor().blockNumber()
                               ? QColor("#c9d1d9") : QColor("#6e7681"));
            painter.drawText(0, top, lineArea_->width() - 9, fontMetrics().height(),
                             Qt::AlignRight, QString::number(number + 1));

            for (const auto &d : diagnostics_) {
                if (d.line != number) continue;
                if (d.severity <= 1) painter.setBrush(QColor("#f14c4c"));
                else if (d.severity == 2) painter.setBrush(QColor("#cca700"));
                else painter.setBrush(QColor("#3794ff"));
                painter.setPen(Qt::NoPen);
                painter.drawEllipse(5, top + fontMetrics().height() / 2 - 2, 4, 4);
                break;
            }
        }
        block = block.next();
        top = bottom;
        bottom = top + qRound(blockBoundingRect(block).height());
        ++number;
    }
}

void CodeEditor::emitCursorPosition() {
    const auto cursor = textCursor();
    Q_EMIT cursorLocationChanged(cursor.blockNumber() + 1,
                                 cursor.positionInBlock() + 1);
}

void CodeEditor::mouseMoveEvent(QMouseEvent *event) {
    QPlainTextEdit::mouseMoveEvent(event);
    if (event->buttons() != Qt::NoButton || textCursor().hasSelection()) {
        hoverTimer_.stop();
        Q_EMIT hoverDismissRequested();
        return;
    }
    Q_EMIT hoverDismissRequested();
    pendingHoverPos_ = event->globalPosition().toPoint();
    hoverTimer_.start();
}

void CodeEditor::mousePressEvent(QMouseEvent *event) {
    hoverTimer_.stop();
    Q_EMIT hoverDismissRequested();
    QPlainTextEdit::mousePressEvent(event);
}

void CodeEditor::leaveEvent(QEvent *event) {
    hoverTimer_.stop();
    Q_EMIT hoverDismissRequested();
    QPlainTextEdit::leaveEvent(event);
}

QString CodeEditor::indentUnit() const {
    return QString(" ").repeated(qMax(2, tabWidth_));
}

QString CodeEditor::lineCommentToken() const {
    switch (language_) {
        case PPIDE_LANG_PUNPUN:
        case PPIDE_LANG_PYTHON:
            return "#";
        case PPIDE_LANG_C:
        case PPIDE_LANG_CPP:
        case PPIDE_LANG_HEADER:
        case PPIDE_LANG_JAVASCRIPT:
            return "//";
        default:
            return {};
    }
}

namespace {
const QString kOpenChars = "([{\"'";
const QString kCloseChars = ")]}\"'";

QString leadingWhitespace(const QString &line) {
    int i = 0;
    while (i < line.size() && (line.at(i) == ' ' || line.at(i) == '\t')) ++i;
    return line.left(i);
}
}

bool CodeEditor::handleNewline() {
    if (!autoIndent_) return false;
    auto cursor = textCursor();
    const QString line = cursor.block().text();
    const int column = cursor.positionInBlock();
    QString indent = leadingWhitespace(line);

    const QString before = line.left(column).trimmed();
    const QString after = line.mid(column).trimmed();

    // PunPun opens blocks with a trailing ':'; C-likes use '{'.
    const bool opensBlock = before.endsWith('{') || before.endsWith(':') ||
                            before.endsWith('(') || before.endsWith('[');
    QString extra = opensBlock ? indentUnit() : QString();

    cursor.beginEditBlock();
    cursor.insertText("\n" + indent + extra);
    // Put a matching close brace on its own line, cursor in the opened body.
    if (before.endsWith('{') && after.startsWith('}')) {
        const int resume = cursor.position();
        cursor.insertText("\n" + indent);
        cursor.setPosition(resume);
    }
    cursor.endEditBlock();
    setTextCursor(cursor);
    return true;
}

bool CodeEditor::handleCloseCharacter(QKeyEvent *event) {
    if (!autoClose_) return false;
    const QString text = event->text();
    if (text.size() != 1) return false;
    auto cursor = textCursor();

    // Typing a closer that is already there just steps over it.
    if (kCloseChars.contains(text) && !cursor.hasSelection()) {
        const QString line = cursor.block().text();
        const int column = cursor.positionInBlock();
        if (column < line.size() && line.at(column) == text.at(0)) {
            cursor.movePosition(QTextCursor::Right);
            setTextCursor(cursor);
            return true;
        }
    }

    if (!kOpenChars.contains(text)) return false;
    const QString partner = QString(kCloseChars.at(kOpenChars.indexOf(text)));

    // Wrap a selection in the pair rather than replacing it.
    if (cursor.hasSelection()) {
        const QString selected = cursor.selectedText();
        const int start = cursor.selectionStart();
        cursor.beginEditBlock();
        cursor.insertText(text + selected + partner);
        cursor.endEditBlock();
        cursor.setPosition(start + 1);
        cursor.setPosition(start + 1 + selected.size(), QTextCursor::KeepAnchor);
        setTextCursor(cursor);
        return true;
    }

    // Only auto-close when the caret is at a natural boundary, so typing an
    // apostrophe inside a word does not sprout a stray quote.
    const QString line = cursor.block().text();
    const int column = cursor.positionInBlock();
    const QChar next = column < line.size() ? line.at(column) : QChar(' ');
    if (!(next.isSpace() || kCloseChars.contains(next))) return false;
    if ((text == "\"" || text == "'") && column > 0) {
        const QChar previous = line.at(column - 1);
        if (previous.isLetterOrNumber() || previous == '_') return false;
    }

    cursor.beginEditBlock();
    cursor.insertText(text + partner);
    cursor.endEditBlock();
    cursor.movePosition(QTextCursor::Left);
    setTextCursor(cursor);
    return true;
}

bool CodeEditor::handleBackspacePair() {
    if (!autoClose_) return false;
    auto cursor = textCursor();
    if (cursor.hasSelection()) return false;
    const QString line = cursor.block().text();
    const int column = cursor.positionInBlock();
    if (column == 0 || column >= line.size() + 1) return false;

    const QChar previous = line.at(column - 1);
    const QChar next = column < line.size() ? line.at(column) : QChar();
    const int index = kOpenChars.indexOf(previous);
    if (index >= 0 && next == kCloseChars.at(index)) {
        cursor.beginEditBlock();
        cursor.deletePreviousChar();
        cursor.deleteChar();
        cursor.endEditBlock();
        return true;
    }

    // Backspace at the start of an indented line removes a whole indent step.
    const QString head = line.left(column);
    if (!head.isEmpty() && head.trimmed().isEmpty()) {
        const int unit = qMax(2, tabWidth_);
        const int remove = ((column - 1) % unit) + 1;
        cursor.beginEditBlock();
        for (int i = 0; i < remove; ++i) cursor.deletePreviousChar();
        cursor.endEditBlock();
        return true;
    }
    return false;
}

void CodeEditor::shiftLines(bool forward) {
    auto cursor = textCursor();
    const int startPos = cursor.selectionStart();
    const int endPos = cursor.selectionEnd();

    QTextCursor probe(document());
    probe.setPosition(startPos);
    const int firstBlock = probe.blockNumber();
    probe.setPosition(endPos);
    int lastBlock = probe.blockNumber();
    // A selection ending exactly at a line start does not include that line.
    if (lastBlock > firstBlock && probe.positionInBlock() == 0) --lastBlock;

    const QString unit = indentUnit();
    cursor.beginEditBlock();
    for (int number = firstBlock; number <= lastBlock; ++number) {
        auto block = document()->findBlockByNumber(number);
        if (!block.isValid()) continue;
        QTextCursor edit(block);
        if (forward) {
            if (block.text().isEmpty()) continue;
            edit.insertText(unit);
        } else {
            const QString text = block.text();
            int remove = 0;
            while (remove < unit.size() && remove < text.size() && text.at(remove) == ' ') ++remove;
            if (remove == 0 && !text.isEmpty() && text.at(0) == '\t') remove = 1;
            for (int i = 0; i < remove; ++i) edit.deleteChar();
        }
    }
    cursor.endEditBlock();

    // Reselect the same lines so the shortcut can be held down.
    auto first = document()->findBlockByNumber(firstBlock);
    auto last = document()->findBlockByNumber(lastBlock);
    QTextCursor result(document());
    result.setPosition(first.position());
    result.setPosition(last.position() + last.length() - 1, QTextCursor::KeepAnchor);
    setTextCursor(result);
}

void CodeEditor::indentSelection() { shiftLines(true); }
void CodeEditor::dedentSelection() { shiftLines(false); }

void CodeEditor::toggleLineComment() {
    const QString token = lineCommentToken();
    if (token.isEmpty()) return;

    auto cursor = textCursor();
    QTextCursor probe(document());
    probe.setPosition(cursor.selectionStart());
    const int firstBlock = probe.blockNumber();
    probe.setPosition(cursor.selectionEnd());
    int lastBlock = probe.blockNumber();
    if (lastBlock > firstBlock && probe.positionInBlock() == 0) --lastBlock;

    // Uncomment only when every non-blank line in range is already commented.
    bool allCommented = true;
    int commonIndent = INT_MAX;
    for (int number = firstBlock; number <= lastBlock; ++number) {
        const QString text = document()->findBlockByNumber(number).text();
        if (text.trimmed().isEmpty()) continue;
        commonIndent = qMin(commonIndent, int(leadingWhitespace(text).size()));
        if (!text.trimmed().startsWith(token)) allCommented = false;
    }
    if (commonIndent == INT_MAX) commonIndent = 0;

    cursor.beginEditBlock();
    for (int number = firstBlock; number <= lastBlock; ++number) {
        auto block = document()->findBlockByNumber(number);
        const QString text = block.text();
        if (text.trimmed().isEmpty()) continue;
        QTextCursor edit(block);
        if (allCommented) {
            const int at = text.indexOf(token);
            edit.setPosition(block.position() + at);
            for (int i = 0; i < token.size(); ++i) edit.deleteChar();
            if (block.text().mid(at).startsWith(' ')) edit.deleteChar();
        } else {
            edit.setPosition(block.position() + commonIndent);
            edit.insertText(token + " ");
        }
    }
    cursor.endEditBlock();
}

void CodeEditor::duplicateSelection() {
    auto cursor = textCursor();
    cursor.beginEditBlock();
    if (cursor.hasSelection()) {
        const QString selected = cursor.selectedText();
        const int end = cursor.selectionEnd();
        cursor.setPosition(end);
        cursor.insertText(selected);
    } else {
        const QString line = cursor.block().text();
        const int column = cursor.positionInBlock();
        cursor.movePosition(QTextCursor::EndOfBlock);
        cursor.insertText("\n" + line);
        cursor.movePosition(QTextCursor::StartOfBlock);
        cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, column);
    }
    cursor.endEditBlock();
    setTextCursor(cursor);
}

void CodeEditor::moveSelectedLines(int delta) {
    if (delta == 0) return;
    auto cursor = textCursor();
    QTextCursor probe(document());
    probe.setPosition(cursor.selectionStart());
    const int firstBlock = probe.blockNumber();
    probe.setPosition(cursor.selectionEnd());
    int lastBlock = probe.blockNumber();
    if (lastBlock > firstBlock && probe.positionInBlock() == 0) --lastBlock;

    const int target = delta < 0 ? firstBlock - 1 : lastBlock + 1;
    if (target < 0 || target >= document()->blockCount()) return;

    QStringList moved;
    for (int number = firstBlock; number <= lastBlock; ++number)
        moved << document()->findBlockByNumber(number).text();
    const QString neighbour = document()->findBlockByNumber(target).text();

    const int low = qMin(target, firstBlock);
    const int high = qMax(target, lastBlock);
    QStringList rebuilt;
    if (delta < 0) { rebuilt = moved; rebuilt << neighbour; }
    else { rebuilt << neighbour; rebuilt += moved; }

    auto lowBlock = document()->findBlockByNumber(low);
    auto highBlock = document()->findBlockByNumber(high);
    QTextCursor edit(document());
    edit.beginEditBlock();
    edit.setPosition(lowBlock.position());
    edit.setPosition(highBlock.position() + highBlock.length() - 1, QTextCursor::KeepAnchor);
    edit.insertText(rebuilt.join("\n"));
    edit.endEditBlock();

    const int newFirst = firstBlock + delta;
    const int newLast = lastBlock + delta;
    auto selFirst = document()->findBlockByNumber(newFirst);
    auto selLast = document()->findBlockByNumber(newLast);
    if (!selFirst.isValid() || !selLast.isValid()) return;
    QTextCursor result(document());
    result.setPosition(selFirst.position());
    result.setPosition(selLast.position() + selLast.length() - 1, QTextCursor::KeepAnchor);
    setTextCursor(result);
}

void CodeEditor::prepareForSave() {
    if (!trimOnSave_ && !finalNewline_) return;
    QTextCursor edit(document());
    edit.beginEditBlock();
    if (trimOnSave_) {
        for (int number = 0; number < document()->blockCount(); ++number) {
            auto block = document()->findBlockByNumber(number);
            const QString text = block.text();
            int end = text.size();
            while (end > 0 && (text.at(end - 1) == ' ' || text.at(end - 1) == '\t')) --end;
            if (end == text.size()) continue;
            QTextCursor line(block);
            line.setPosition(block.position() + end);
            line.setPosition(block.position() + text.size(), QTextCursor::KeepAnchor);
            line.removeSelectedText();
        }
    }
    if (finalNewline_ && !toPlainText().endsWith('\n')) {
        edit.movePosition(QTextCursor::End);
        edit.insertText("\n");
    }
    edit.endEditBlock();
}

void CodeEditor::setEditingBehaviour(bool autoIndent, bool autoCloseBrackets,
                                     bool highlightCurrentLine, bool showWhitespace,
                                     bool trimTrailingWhitespaceOnSave,
                                     bool insertFinalNewline) {
    autoIndent_ = autoIndent;
    autoClose_ = autoCloseBrackets;
    highlightLine_ = highlightCurrentLine;
    trimOnSave_ = trimTrailingWhitespaceOnSave;
    finalNewline_ = insertFinalNewline;

    QTextOption option = document()->defaultTextOption();
    option.setFlags(showWhitespace
                        ? option.flags() | QTextOption::ShowTabsAndSpaces
                        : option.flags() & ~QTextOption::ShowTabsAndSpaces);
    document()->setDefaultTextOption(option);
    refreshDecorations();
}

void CodeEditor::setEditorPalette(const QColor &lineHighlight, const QColor &selection,
                                  const QColor &muted, const QColor &accent) {
    lineHighlightColor_ = lineHighlight;
    selectionColor_ = selection;
    mutedColor_ = muted;
    accentColor_ = accent;
    refreshDecorations();
    lineArea_->update();
}

void CodeEditor::keyPressEvent(QKeyEvent *event) {
    if (completer_->popup()->isVisible() &&
        (event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return ||
         event->key() == Qt::Key_Tab || event->key() == Qt::Key_Escape)) {
        event->ignore();
        return;
    }

    const bool control = event->modifiers() & Qt::ControlModifier;
    const bool shift = event->modifiers() & Qt::ShiftModifier;
    const bool alt = event->modifiers() & Qt::AltModifier;

    if (control && event->key() == Qt::Key_Space) { showCompletion(); return; }
    if (control && event->key() == Qt::Key_Slash) { toggleLineComment(); return; }
    if (control && shift && event->key() == Qt::Key_D) { duplicateSelection(); return; }
    if (alt && event->key() == Qt::Key_Up) { moveSelectedLines(-1); return; }
    if (alt && event->key() == Qt::Key_Down) { moveSelectedLines(1); return; }

    if (event->key() == Qt::Key_Backtab || (event->key() == Qt::Key_Tab && shift)) {
        dedentSelection();
        return;
    }

    if (event->key() == Qt::Key_Tab && !control) {
        // Indent the block when lines are selected; never replace the selection.
        if (textCursor().hasSelection()) indentSelection();
        else insertPlainText(indentUnit());
        return;
    }

    if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) &&
        !control && !shift && handleNewline()) {
        return;
    }

    if (event->key() == Qt::Key_Backspace && !control && handleBackspacePair()) return;

    if (!control && !alt && handleCloseCharacter(event)) return;

    QPlainTextEdit::keyPressEvent(event);
}
