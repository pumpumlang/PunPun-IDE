#include "SmartAnalyzer.h"

#include "PunPunLanguage.h"

#include <QRegularExpression>
#include <QStringList>

namespace {
EditorDiagnostic diagnostic(int line, int column, int length, int severity,
                            const QString &message, const QString &code,
                            const QString &suggestion) {
    EditorDiagnostic d;
    d.line = qMax(0, line);
    d.column = qMax(0, column);
    d.length = qMax(1, length);
    d.severity = severity;
    d.message = message;
    d.code = code;
    d.source = "Smart Check";
    d.suggestion = suggestion;
    return d;
}

void addRegexHints(QVector<EditorDiagnostic> &out, const QString &text,
                   const QRegularExpression &rx, int severity,
                   const QString &message, const QString &code,
                   const QString &suggestion, int group = 0) {
    auto it = rx.globalMatch(text);
    while (it.hasNext()) {
        const auto m = it.next();
        // `group` lets a rule anchor on context it does not want to underline,
        // such as the line start before a migration keyword.
        const int absolute = m.capturedStart(group);
        if (absolute < 0) continue;
        const QString before = text.left(absolute);
        const int line = before.count('\n');
        const int lastBreak = before.lastIndexOf('\n');
        const int column = absolute - (lastBreak + 1);
        out.push_back(diagnostic(line, column, m.capturedLength(group), severity,
                                 message, code, suggestion));
    }
}

/// A copy of `text` with string literals and comments blanked to spaces.
///
/// Length and every offset are preserved, so a diagnostic found in the masked
/// copy points at the right place in the real document. Word-level rules run
/// against this: without it, "no" in a sentence and `craft` in a comment were
/// both reported as code, which buried the real findings.
QString maskedCode(const QString &text) {
    QString out = text;
    bool inString = false;
    QChar quote;
    bool escaped = false;
    bool lineComment = false;
    bool blockComment = false;

    auto blank = [&out](qsizetype index) {
        if (out.at(index) != '\n') out[index] = ' ';
    };

    for (qsizetype i = 0; i < text.size(); ++i) {
        const QChar ch = text.at(i);
        const QChar next = i + 1 < text.size() ? text.at(i + 1) : QChar();

        if (lineComment) {
            if (ch == '\n') lineComment = false;
            else blank(i);
            continue;
        }
        if (blockComment) {
            if (ch == '*' && next == '/') {
                blank(i);
                blank(i + 1);
                ++i;
                blockComment = false;
            } else {
                blank(i);
            }
            continue;
        }
        if (inString) {
            blank(i);
            if (escaped) escaped = false;
            else if (ch == '\\') escaped = true;
            else if (ch == quote) inString = false;
            continue;
        }

        if (ch == '/' && next == '/') { lineComment = true; blank(i); blank(i + 1); ++i; continue; }
        if (ch == '/' && next == '*') { blockComment = true; blank(i); blank(i + 1); ++i; continue; }
        if (ch == '#') { lineComment = true; blank(i); continue; }
        if (ch == '"' || ch == '\'') { inString = true; quote = ch; blank(i); continue; }
    }
    return out;
}

QVector<EditorDiagnostic> delimiterHints(const QString &text) {
    QVector<EditorDiagnostic> out;
    struct Open { QChar ch; int line; int column; };
    QVector<Open> stack;
    bool inString = false;
    QChar quote;
    bool escaped = false;
    bool lineComment = false;
    bool blockComment = false;
    int line = 0;
    int column = 0;

    auto matches = [](QChar open, QChar close) {
        return (open == '(' && close == ')') ||
               (open == '[' && close == ']') ||
               (open == '{' && close == '}');
    };

    for (qsizetype i = 0; i < text.size(); ++i) {
        const QChar ch = text.at(i);
        const QChar next = i + 1 < text.size() ? text.at(i + 1) : QChar();

        if (ch == '\n') {
            lineComment = false;
            ++line;
            column = 0;
            escaped = false;
            continue;
        }

        if (lineComment) {
            ++column;
            continue;
        }
        if (blockComment) {
            if (ch == '*' && next == '/') {
                blockComment = false;
                ++i;
                column += 2;
            } else {
                ++column;
            }
            continue;
        }
        if (inString) {
            if (escaped) {
                escaped = false;
            } else if (ch == '\\') {
                escaped = true;
            } else if (ch == quote) {
                inString = false;
            }
            ++column;
            continue;
        }

        if (ch == '/' && next == '/') {
            lineComment = true;
            ++i;
            column += 2;
            continue;
        }
        if (ch == '/' && next == '*') {
            blockComment = true;
            ++i;
            column += 2;
            continue;
        }
        if (ch == '#' && (column == 0 || text.at(i - 1).isSpace())) {
            // PunPun/Python style line comment. C preprocessor lines are harmless
            // here because delimiters inside them rarely affect the file body.
            lineComment = true;
            ++column;
            continue;
        }
        if (ch == '"' || ch == '\'') {
            inString = true;
            quote = ch;
            ++column;
            continue;
        }
        if (ch == '(' || ch == '[' || ch == '{') {
            stack.push_back({ch, line, column});
        } else if (ch == ')' || ch == ']' || ch == '}') {
            if (stack.isEmpty() || !matches(stack.constLast().ch, ch)) {
                out.push_back(diagnostic(line, column, 1, 1,
                                         QString("Unmatched closing '%1'.").arg(ch),
                                         "SC1001", "Remove it or add the matching opening delimiter."));
            } else {
                stack.removeLast();
            }
        }
        ++column;
    }

    for (const auto &open : stack) {
        out.push_back(diagnostic(open.line, open.column, 1, 1,
                                 QString("Unclosed '%1'.").arg(open.ch),
                                 "SC1002", "Add the matching closing delimiter."));
    }
    return out;
}
}

QVector<EditorDiagnostic> SmartAnalyzer::analyze(ppide_language language,
                                                  const QString &text,
                                                  const QString &) {
    QVector<EditorDiagnostic> out = delimiterHints(text);
    // Language rules read the masked copy so that prose and string contents
    // cannot masquerade as code. The marker rule deliberately reads the raw
    // text: a TODO is normally written in a comment.
    const QString code = maskedCode(text);

    addRegexHints(out, text,
                  QRegularExpression(R"(\b(?:TODO|FIXME|HACK)\b)"), 3,
                  "Unresolved development marker.", "SC0001",
                  "Resolve this marker before release, or turn it into an issue with context.");

    addRegexHints(out, text,
                  QRegularExpression(R"([ \t]+$)", QRegularExpression::MultilineOption), 3,
                  "Trailing whitespace.", "SC0002",
                  "Remove trailing spaces so formatting and diffs stay clean.");

    if (language == PPIDE_LANG_PUNPUN) {
        // These rules used to point the wrong way: they flagged the modern
        // `import` as a mistake and recommended `bring` and `pin`, which are
        // the forms the compiler itself warns about. The migration table is
        // now the compiler's, and each hint names the spelling PPC would.
        for (const auto &form : PunPunLanguage::legacyForms()) {
            const QString word = QRegularExpression::escape(form.legacy);
            QRegularExpression rx(form.statementOnly
                                      ? QString(R"(^[ \t]*(%1)\b)").arg(word)
                                      : QString(R"(\b(%1)\b)").arg(word),
                                  QRegularExpression::MultilineOption);
            addRegexHints(
                out, code, rx, 3,
                QString("`%1` belongs to the migration dialect; PunPun spells this `%2`.")
                    .arg(form.legacy, form.modern),
                "PP1001",
                QString("%1 `pp migrate` rewrites the whole file.").arg(form.advice), 1);
        }
        addRegexHints(out, code,
                      QRegularExpression(R"(\b(?:var|val)\s+[A-Za-z_]\w*)"), 2,
                      "PunPun has no `var`/`val` binding.", "PP1002",
                      "Write `let name = value`, or `let mut name = value` when it is reassigned.");
        addRegexHints(out, code,
                      QRegularExpression(R"(\b(?:NULL|null|nil|nullptr)\b)"), 2,
                      "PunPun has no null; absence is an `Option`.", "PP1005",
                      "Return `Option<T>` and match on it rather than borrowing a null spelling from C or JavaScript.");
        addRegexHints(out, code,
                      QRegularExpression(R"(\bif\s*\([^\n;]+\)\s*;)"), 2,
                      "Suspicious empty conditional body.", "PP1003",
                      "Remove the stray semicolon or add the intended body.");
        addRegexHints(out, code,
                      QRegularExpression(R"(^\s*(?:pub|func|def|fun)\s+[A-Za-z_]\w*\s*\()",
                                         QRegularExpression::MultilineOption), 2,
                      "This is not how PunPun declares a function.", "PP1006",
                      "Write `fn name(argument: Type) -> Result`, and prefix it with `public` to export it.");
        addRegexHints(out, code,
                      QRegularExpression(R"(\bfn\s+[A-Za-z_]\w*\s*\([^)]*\)\s*:\s*[A-Za-z_])"), 2,
                      "PunPun writes the result type after an arrow.", "PP1007",
                      "Write `fn name() -> Type`, not `fn name(): Type`.");
        addRegexHints(out, code,
                      QRegularExpression(R"(\bfor\s+[A-Za-z_]\w*\s*=\s*\d)"), 2,
                      "PunPun's `for` walks a range or a sequence.", "PP1008",
                      "Write `for i in 0..n` for a count, or `for element in sequence` to iterate directly.");
    }

    if (language == PPIDE_LANG_C || language == PPIDE_LANG_CPP || language == PPIDE_LANG_HEADER) {
        addRegexHints(out, code,
                      QRegularExpression(R"(\busing\s+namespace\s+std\s*;)"), 3,
                      "Global 'using namespace std' can create name collisions.", "CPP1001",
                      "Prefer `std::name` or import only the specific names you need." );
        addRegexHints(out, code,
                      QRegularExpression(R"(\b(?:gets|strcpy|strcat|sprintf)\s*\()"), 2,
                      "Potentially unsafe C string operation.", "C1002",
                      "Use a bounded alternative or a C++ string/container API where possible." );
        addRegexHints(out, code,
                      QRegularExpression(R"(\bif\s*\([^\n;]+\)\s*;)"), 2,
                      "Suspicious empty if statement.", "C1003",
                      "Check for a stray semicolon after the condition." );
        if (language != PPIDE_LANG_C) {
            addRegexHints(out, code,
                          QRegularExpression(R"(\bmalloc\s*\()"), 3,
                          "Raw C allocation in C++ code.", "CPP1004",
                          "Prefer RAII containers or smart pointers unless this is deliberate interop code." );
            addRegexHints(out, code,
                          QRegularExpression(R"(\bnew\s+[A-Za-z_:])"), 3,
                          "Raw owning allocation deserves a lifetime check.", "CPP1005",
                          "Prefer stack ownership, a container, or `std::make_unique` unless raw ownership is intentional." );
        }
    }

    return out;
}

QString SmartAnalyzer::suggestionFor(ppide_language language, const QString &message) {
    const QString m = message.toLower();
    if (m.contains("not declared") || m.contains("unknown name") || m.contains("undeclared"))
        return "Check the spelling, scope, and required import/include. PunPun PPC may also provide a machine-applicable fix-it.";
    if (m.contains("expected") && m.contains(";"))
        return "The parser expected a statement terminator here. Check the previous line too, because the real mistake is often just before the caret.";
    if (m.contains("incomplete type"))
        return "The type was only forward-declared. Include the header that provides its complete definition before using members or static signals.";
    if (m.contains("unused"))
        return "Remove the unused value, use it, or explicitly mark it intentionally unused.";
    if (m.contains("conversion") || m.contains("narrow"))
        return "Make the conversion explicit and verify the destination type can represent the full value range.";
    if (language == PPIDE_LANG_PUNPUN && m.contains("bring"))
        return "Verify the module name and that the stdlib/module path belongs to the active PunPun toolchain.";
    return "Open the diagnostic location, read the first compiler error, and fix that before chasing follow-on messages.";
}
