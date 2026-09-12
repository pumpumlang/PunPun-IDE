// Tests for the offline analyzer's PunPun rules.
//
// These exist because the rules previously contradicted the compiler: they
// reported the modern `import` as a mistake and recommended `bring`, `pin` and
// `say`-free `print` spellings that PPC answers with a migration warning. A
// hint that points the wrong way is worse than no hint, so the direction of
// each rule is pinned here.
#include "SmartAnalyzer.h"
#include "PunPunLanguage.h"

#include <QTest>

class AnalyzerTests : public QObject {
    Q_OBJECT

private:
    static QVector<EditorDiagnostic> punpun(const QString &source) {
        return SmartAnalyzer::analyze(PPIDE_LANG_PUNPUN, source, "a.pp");
    }

    static bool hasCode(const QVector<EditorDiagnostic> &found, const QString &code) {
        for (const auto &d : found)
            if (d.code == code) return true;
        return false;
    }

    static QString messageFor(const QVector<EditorDiagnostic> &found, const QString &code) {
        for (const auto &d : found)
            if (d.code == code) return d.message;
        return {};
    }

private Q_SLOTS:
    void modernSourceIsClean() {
        const QString source =
            "import std.io\n"
            "\n"
            "fn total(values: List<int>) -> int {\n"
            "    let mut sum = 0;\n"
            "    for value in values { sum = sum + value; }\n"
            "    return sum;\n"
            "}\n"
            "\n"
            "launch {\n"
            "    say(total(list<int>()));\n"
            "}\n";
        const auto found = punpun(source);
        for (const auto &d : found)
            QVERIFY2(!d.code.startsWith("PP"),
                     qPrintable(QString("modern source flagged: %1 %2").arg(d.code, d.message)));
    }

    void migrationDialectIsReportedTowardsTheModernSpelling() {
        const auto found = punpun("craft main() gives int {\n    give 1;\n}\n");
        QVERIFY(hasCode(found, "PP1001"));
        const QString message = messageFor(found, "PP1001");
        QVERIFY2(message.contains("`fn`"), qPrintable(message));
        QVERIFY2(!message.contains("`craft`.") , qPrintable(message));
    }

    void importIsNeverReportedAsLegacy() {
        // The rule this replaces said "PunPun 1.3 module imports use 'bring'",
        // which is backwards: `bring` is the form PPC warns about.
        const auto found = punpun("import std.io\n\nlaunch { say(\"x\"); }\n");
        QCOMPARE(hasCode(found, "PP1001"), false);
    }

    void bringIsReportedAsLegacy() {
        const auto found = punpun("bring std.io\n\nlaunch { say(\"x\"); }\n");
        QVERIFY(hasCode(found, "PP1001"));
        QVERIFY(messageFor(found, "PP1001").contains("`import`"));
    }

    void prosePassesThroughUnflagged() {
        // "no", "give" and "each" are English before they are keywords. Inside
        // a string or a comment they are not code at all.
        const QString source =
            "# each of these is fine, and no, we do not give up\n"
            "launch {\n"
            "    say(\"answer: no\");\n"
            "    say(\"give me each value\");\n"
            "}\n";
        const auto found = punpun(source);
        QCOMPARE(hasCode(found, "PP1001"), false);
    }

    void ambiguousKeywordsOnlyCountAtStatementStart() {
        // A call named `give(...)` mid-expression is not the migration keyword.
        QCOMPARE(hasCode(punpun("launch { let x = give(1); }\n"), "PP1001"), false);
        QVERIFY(hasCode(punpun("fn f() -> int {\n    give 1;\n}\n"), "PP1001"));
    }

    void foreignFunctionSyntaxIsNamed() {
        QVERIFY(hasCode(punpun("def main():\n    pass\n"), "PP1006"));
        QVERIFY(hasCode(punpun("fn main(): int { return 1; }\n"), "PP1007"));
    }

    void nullSpellingsAreReported() {
        QVERIFY(hasCode(punpun("launch { let x = nullptr; }\n"), "PP1005"));
        // ...but not when they are only mentioned in prose.
        QCOMPARE(hasCode(punpun("# null is not a PunPun concept\nlaunch { }\n"), "PP1005"), false);
    }

    void vocabularyMatchesTheCompiler() {
        const QStringList words = PunPunLanguage::completionWords();
        // Present: the spellings the grammar and builtin table actually define.
        for (const QString &expected : {"import", "contract", "object", "meets", "fn", "say", "list_push"})
            QVERIFY2(words.contains(expected), qPrintable("missing: " + expected));
        // Absent: forms the old hand-written list invented or kept past their life.
        for (const QString &unwanted : {"trait", "impl", "bring", "craft", "pin", "done", "min"})
            QVERIFY2(!words.contains(unwanted), qPrintable("should not be offered: " + unwanted));
    }
};

QTEST_MAIN(AnalyzerTests)
#include "test_analyzer.moc"
