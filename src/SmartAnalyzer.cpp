#include "SmartAnalyzer.h"

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
                   const QString &suggestion) {
    auto it = rx.globalMatch(text);
    while (it.hasNext()) {
        const auto m = it.next();
        const int absolute = m.capturedStart();
        const QString before = text.left(absolute);
        const int line = before.count('\n');
        const int lastBreak = before.lastIndexOf('\n');
        const int column = absolute - (lastBreak + 1);
        out.push_back(diagnostic(line, column, m.capturedLength(), severity,
                                 message, code, suggestion));
    }
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

    addRegexHints(out, text,
                  QRegularExpression(R"(\b(?:TODO|FIXME|HACK)\b)"), 3,
                  "Unresolved development marker.", "SC0001",
                  "Resolve this marker before release, or turn it into an issue with context.");

    addRegexHints(out, text,
                  QRegularExpression(R"([ \t]+$)", QRegularExpression::MultilineOption), 3,
                  "Trailing whitespace.", "SC0002",
                  "Remove trailing spaces so formatting and diffs stay clean.");

    if (language == PPIDE_LANG_PUNPUN) {
        addRegexHints(out, text,
                      QRegularExpression(R"(\b(?:use|import)\b)"), 2,
                      "PunPun 1.3 module imports use 'bring'.", "PP1001",
                      "Replace this import form with `bring module.name` (or the migration syntax your project uses)." );
        addRegexHints(out, text,
                      QRegularExpression(R"(\b(?:var)\s+[A-Za-z_]\w*)"), 3,
                      "Prefer PunPun's explicit binding forms.", "PP1002",
                      "Use `let`/`mut` in modern syntax or `pin` in migration syntax so mutability is obvious." );
        addRegexHints(out, text,
                      QRegularExpression(R"(\b(?:print|println)\s*\()"), 2,
                      "PunPun output uses `say` rather than C/Python-style print calls.", "PP1004",
                      "Use `say value` / `say(value)` in the syntax mode your project uses." );
        addRegexHints(out, text,
                      QRegularExpression(R"(\b(?:NULL|null)\b)"), 2,
                      "This null spelling does not match PunPun's option/value syntax.", "PP1005",
                      "Use `none` or an `Option` value instead of importing a C/JavaScript null spelling." );
        addRegexHints(out, text,
                      QRegularExpression(R"(\bif\s*\([^\n;]+\)\s*;)"), 2,
                      "Suspicious empty conditional body.", "PP1003",
                      "Remove the stray semicolon or add the intended body." );
    }

    if (language == PPIDE_LANG_C || language == PPIDE_LANG_CPP || language == PPIDE_LANG_HEADER) {
        addRegexHints(out, text,
                      QRegularExpression(R"(\busing\s+namespace\s+std\s*;)"), 3,
                      "Global 'using namespace std' can create name collisions.", "CPP1001",
                      "Prefer `std::name` or import only the specific names you need." );
        addRegexHints(out, text,
                      QRegularExpression(R"(\b(?:gets|strcpy|strcat|sprintf)\s*\()"), 2,
                      "Potentially unsafe C string operation.", "C1002",
                      "Use a bounded alternative or a C++ string/container API where possible." );
        addRegexHints(out, text,
                      QRegularExpression(R"(\bif\s*\([^\n;]+\)\s*;)"), 2,
                      "Suspicious empty if statement.", "C1003",
                      "Check for a stray semicolon after the condition." );
        if (language != PPIDE_LANG_C) {
            addRegexHints(out, text,
                          QRegularExpression(R"(\bmalloc\s*\()"), 3,
                          "Raw C allocation in C++ code.", "CPP1004",
                          "Prefer RAII containers or smart pointers unless this is deliberate interop code." );
            addRegexHints(out, text,
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
