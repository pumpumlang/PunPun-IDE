// Behavioural tests for CodeEditor. These cover the editing rules a code
// editor is judged by: indentation that survives Enter, bracket pairing that
// does not fight the typist, block indent that keeps the selection, comment
// toggling, and save-time normalisation.
#include "CodeEditor.h"

#include <QTest>
#include <QTextCursor>

class EditorTests : public QObject {
    Q_OBJECT

private:
    static CodeEditor *makeEditor(const QString &path, const QString &text = {}) {
        auto *editor = new CodeEditor;
        editor->setFilePath(path);
        editor->configure(12, "monospace", 4, false, nullptr);
        editor->setEditingBehaviour(true, true, true, false, true, true);
        editor->setPlainText(text);
        return editor;
    }

    static void typeText(CodeEditor *editor, const QString &text) {
        for (const QChar &ch : text) {
            if (ch == '\n') QTest::keyClick(editor, Qt::Key_Return);
            else QTest::keyClicks(editor, QString(ch));
        }
    }

private Q_SLOTS:
    void newlineKeepsIndent() {
        auto *editor = makeEditor("a.pp", "    say \"one\"");
        auto cursor = editor->textCursor();
        cursor.movePosition(QTextCursor::End);
        editor->setTextCursor(cursor);
        QTest::keyClick(editor, Qt::Key_Return);
        QTest::keyClicks(editor, "x");
        QCOMPARE(editor->toPlainText(), QString("    say \"one\"\n    x"));
        delete editor;
    }

    void newlineIndentsAfterBlockOpener() {
        // PunPun opens a block with a trailing colon.
        auto *editor = makeEditor("a.pp", "launch:");
        auto cursor = editor->textCursor();
        cursor.movePosition(QTextCursor::End);
        editor->setTextCursor(cursor);
        QTest::keyClick(editor, Qt::Key_Return);
        QTest::keyClicks(editor, "say");
        QCOMPARE(editor->toPlainText(), QString("launch:\n    say"));
        delete editor;
    }

    void bracketsAutoCloseAndStepOver() {
        auto *editor = makeEditor("a.c", "");
        QTest::keyClicks(editor, "f(");
        QCOMPARE(editor->toPlainText(), QString("f()"));
        // Typing the closer should step over the inserted one, not double it.
        QTest::keyClicks(editor, ")");
        QCOMPARE(editor->toPlainText(), QString("f()"));
        QCOMPARE(editor->textCursor().positionInBlock(), 3);
        delete editor;
    }

    void quoteInsideWordDoesNotAutoClose() {
        auto *editor = makeEditor("a.c", "");
        QTest::keyClicks(editor, "dont");
        QTest::keyClicks(editor, "'");
        QCOMPARE(editor->toPlainText(), QString("dont'"));
        delete editor;
    }

    void backspaceRemovesBracketPair() {
        auto *editor = makeEditor("a.c", "");
        QTest::keyClicks(editor, "(");
        QCOMPARE(editor->toPlainText(), QString("()"));
        QTest::keyClick(editor, Qt::Key_Backspace);
        QCOMPARE(editor->toPlainText(), QString(""));
        delete editor;
    }

    void tabIndentsSelectionInsteadOfReplacingIt() {
        auto *editor = makeEditor("a.pp", "one\ntwo");
        editor->selectAll();
        QTest::keyClick(editor, Qt::Key_Tab);
        QCOMPARE(editor->toPlainText(), QString("    one\n    two"));
        // The selection must survive so the shortcut can be repeated.
        QVERIFY(editor->textCursor().hasSelection());
        QTest::keyClick(editor, Qt::Key_Backtab);
        QCOMPARE(editor->toPlainText(), QString("one\ntwo"));
        delete editor;
    }

    void commentTogglesOnAndOff() {
        auto *editor = makeEditor("a.pp", "    say \"one\"\n    say \"two\"");
        editor->selectAll();
        editor->toggleLineComment();
        QCOMPARE(editor->toPlainText(),
                 QString("    # say \"one\"\n    # say \"two\""));
        editor->selectAll();
        editor->toggleLineComment();
        QCOMPARE(editor->toPlainText(), QString("    say \"one\"\n    say \"two\""));
        delete editor;
    }

    void commentUsesLanguageToken() {
        auto *editor = makeEditor("a.cpp", "int x;");
        editor->selectAll();
        editor->toggleLineComment();
        QCOMPARE(editor->toPlainText(), QString("// int x;"));
        delete editor;
    }

    void moveLinesReordersAndKeepsSelection() {
        auto *editor = makeEditor("a.pp", "first\nsecond\nthird");
        auto cursor = editor->textCursor();
        cursor.setPosition(0);
        editor->setTextCursor(cursor);
        editor->moveSelectedLines(1);
        QCOMPARE(editor->toPlainText(), QString("second\nfirst\nthird"));
        delete editor;
    }

    void duplicateCopiesCurrentLine() {
        auto *editor = makeEditor("a.pp", "value");
        editor->duplicateSelection();
        QCOMPARE(editor->toPlainText(), QString("value\nvalue"));
        delete editor;
    }

    void saveTrimsTrailingWhitespaceAndAddsNewline() {
        auto *editor = makeEditor("a.pp", "say \"x\"   \n  trailing\t");
        editor->prepareForSave();
        QCOMPARE(editor->toPlainText(), QString("say \"x\"\n  trailing\n"));
        delete editor;
    }

    void saveNormalisationCanBeDisabled() {
        auto *editor = makeEditor("a.pp", "keep me   ");
        editor->setEditingBehaviour(true, true, true, false, false, false);
        editor->prepareForSave();
        QCOMPARE(editor->toPlainText(), QString("keep me   "));
        delete editor;
    }
};

QTEST_MAIN(EditorTests)
#include "test_editor.moc"
