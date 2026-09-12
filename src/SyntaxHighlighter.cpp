#include "SyntaxHighlighter.h"

#include "PunPunLanguage.h"

#include <QColor>
#include <QFont>
#include <QTextDocument>

namespace {
QTextCharFormat makeFormat(const char *color, bool bold = false, bool italic = false) {
    QTextCharFormat format;
    format.setForeground(QColor(color));
    format.setFontWeight(bold ? QFont::DemiBold : QFont::Normal);
    format.setFontItalic(italic);
    return format;
}
}

SyntaxHighlighter::SyntaxHighlighter(QTextDocument *doc, ppide_language language,
                                     const QStringList &extraKeywords)
    : QSyntaxHighlighter(doc), language_(language) {
    rebuild(extraKeywords);
}

void SyntaxHighlighter::setLanguage(ppide_language language,
                                    const QStringList &extraKeywords) {
    language_ = language;
    rebuild(extraKeywords);
    rehighlight();
}

void SyntaxHighlighter::addWords(const QStringList &words,
                                 const QTextCharFormat &format) {
    if (words.isEmpty()) return;
    QStringList escaped;
    escaped.reserve(words.size());
    for (const auto &word : words) escaped << QRegularExpression::escape(word);
    rules_.push_back({QRegularExpression("\\b(?:" + escaped.join('|') + ")\\b"), format});
}

void SyntaxHighlighter::rebuild(const QStringList &extraKeywords) {
    rules_.clear();

    // VS Code Dark Modern inspired token roles. These are deliberately muted,
    // with green reserved for comments/success rather than painting the UI neon.
    commentFmt_ = makeFormat("#6A9955", false, true);
    stringFmt_ = makeFormat("#CE9178");
    numberFmt_ = makeFormat("#B5CEA8");
    keywordFmt_ = makeFormat("#C586C0", true);
    typeFmt_ = makeFormat("#4EC9B0");
    preprocessorFmt_ = makeFormat("#C586C0");
    headingFmt_ = makeFormat("#569CD6", true);
    fenceFmt_ = makeFormat("#808080", false, true);
    escapeFmt_ = makeFormat("#D7BA7D");
    propertyFmt_ = makeFormat("#9CDCFE");

    const auto declarationFmt = makeFormat("#569CD6", true);
    const auto functionFmt = makeFormat("#DCDCAA");
    const auto builtinFmt = makeFormat("#9CDCFE");
    const auto constantFmt = makeFormat("#4FC1FF");
    const auto moduleFmt = makeFormat("#C8C8C8");

    if (language_ == PPIDE_LANG_PUNPUN) {
        QStringList flow = PunPunLanguage::flowKeywords();
        QStringList declaration = PunPunLanguage::declarationKeywords();
        // The migration dialect is painted too -- it still compiles, and a file
        // mid-migration should not go monochrome -- but in the muted comment
        // colour, so legacy spellings read as something to move off.
        QStringList legacy;
        for (const auto &form : PunPunLanguage::legacyForms()) legacy << form.legacy;
        legacy << "done" << "from" << "until" << "sealed";

        flow << extraKeywords;
        addWords(flow, keywordFmt_);
        addWords(declaration, declarationFmt);
        addWords(PunPunLanguage::typeNames(), typeFmt_);
        addWords(PunPunLanguage::constants(), constantFmt);
        addWords(PunPunLanguage::builtins(), builtinFmt);
        addWords(legacy, makeFormat("#8A8A8A", false, true));

        rules_.push_back({QRegularExpression(R"(\b(?:fn|craft)\s+([A-Za-z_]\w*))"), functionFmt, 1});
        rules_.push_back({QRegularExpression(R"(\b(?:struct|object|enum|contract|shape)\s+([A-Za-z_]\w*))"), typeFmt_, 1});
        rules_.push_back({QRegularExpression(R"(\bmeets\s+([A-Za-z_]\w*))"), typeFmt_, 1});
        rules_.push_back({QRegularExpression(R"(\b(?:let|mut|pin|keep|const)\s+([A-Za-z_]\w*))"), builtinFmt, 1});
        rules_.push_back({QRegularExpression(R"(\b(?:import|bring)\s+([A-Za-z_][\w.:]*))"), moduleFmt, 1});
        rules_.push_back({QRegularExpression(R"(\b([A-Za-z_]\w*)(?=::))"), typeFmt_, 1});
        rules_.push_back({QRegularExpression(R"((?:::|\.)\s*([A-Za-z_]\w*))"), propertyFmt_, 1});
        rules_.push_back({QRegularExpression(R"(\b([A-Za-z_]\w*)\s*(?=\())"), functionFmt, 1});
    } else if (language_ == PPIDE_LANG_C || language_ == PPIDE_LANG_CPP ||
               language_ == PPIDE_LANG_HEADER) {
        QStringList flow = {"break", "case", "continue", "default", "do", "else", "for",
                            "goto", "if", "return", "switch", "while", "try", "catch",
                            "throw", "co_await", "co_return", "co_yield"};
        QStringList declaration = {"class", "struct", "enum", "union", "namespace", "template",
                                   "typename", "using", "typedef", "concept", "requires",
                                   "public", "private", "protected", "virtual", "override",
                                   "extern", "static", "inline", "constexpr", "consteval",
                                   "constinit", "friend", "explicit", "mutable"};
        flow << extraKeywords;
        addWords(flow, keywordFmt_);
        addWords(declaration, declarationFmt);
        addWords({"void", "bool", "char", "wchar_t", "char8_t", "char16_t", "char32_t",
                  "short", "int", "long", "float", "double", "signed", "unsigned", "auto",
                  "size_t", "ptrdiff_t", "uint8_t", "uint16_t", "uint32_t", "uint64_t",
                  "int8_t", "int16_t", "int32_t", "int64_t", "std", "string", "string_view",
                  "vector", "array", "map", "unordered_map", "set", "optional", "variant",
                  "unique_ptr", "shared_ptr", "weak_ptr"}, typeFmt_);
        addWords({"true", "false", "nullptr", "this"}, constantFmt);
        addWords({"new", "delete", "sizeof", "alignof", "decltype", "noexcept", "typeid"}, keywordFmt_);
        rules_.push_back({QRegularExpression(R"(^\s*#\s*(include|define|if|ifdef|ifndef|elif|else|endif|pragma|error|warning)\b)"), preprocessorFmt_, 1});
        rules_.push_back({QRegularExpression(R"(#\s*include\s*[<\"]([^>\"]+)[>\"])"), stringFmt_, 1});
        rules_.push_back({QRegularExpression(R"(\b(?:class|struct|enum|union|namespace)\s+([A-Za-z_]\w*))"), typeFmt_, 1});
        rules_.push_back({QRegularExpression(R"(\b([A-Za-z_]\w*)(?=::))"), typeFmt_, 1});
        rules_.push_back({QRegularExpression(R"((?:\.|->)\s*([A-Za-z_]\w*))"), propertyFmt_, 1});
        rules_.push_back({QRegularExpression(R"(\b([A-Z_][A-Z0-9_]{2,})\b)"), constantFmt, 1});
        rules_.push_back({QRegularExpression(R"(\b([A-Za-z_]\w*)\s*(?=\())"), functionFmt, 1});
    } else if (language_ == PPIDE_LANG_JAVASCRIPT) {
        addWords({"const", "let", "var", "function", "return", "if", "else", "for", "while",
                  "class", "extends", "new", "async", "await", "import", "export", "from",
                  "try", "catch", "throw", "switch", "case", "break"}, keywordFmt_);
        addWords({"true", "false", "null", "undefined"}, constantFmt);
        rules_.push_back({QRegularExpression(R"(\bfunction\s+([A-Za-z_$][\w$]*))"), functionFmt, 1});
        rules_.push_back({QRegularExpression(R"(\.\s*([A-Za-z_$][\w$]*))"), propertyFmt_, 1});
        rules_.push_back({QRegularExpression(R"(\b([A-Za-z_$][\w$]*)\s*(?=\())"), functionFmt, 1});
    } else if (language_ == PPIDE_LANG_PYTHON) {
        addWords({"def", "class", "return", "if", "elif", "else", "for", "while", "import",
                  "from", "as", "async", "await", "try", "except", "finally", "raise", "with",
                  "lambda", "yield", "and", "or", "not", "in", "is", "pass", "break", "continue"}, keywordFmt_);
        addWords({"True", "False", "None"}, constantFmt);
        rules_.push_back({QRegularExpression(R"(\bdef\s+([A-Za-z_]\w*))"), functionFmt, 1});
        rules_.push_back({QRegularExpression(R"(^\s*@([A-Za-z_]\w*(?:\.[A-Za-z_]\w*)*))"), preprocessorFmt_, 1});
        rules_.push_back({QRegularExpression(R"(\bself\.([A-Za-z_]\w*))"), propertyFmt_, 1});
        rules_.push_back({QRegularExpression(R"(\bclass\s+([A-Za-z_]\w*))"), typeFmt_, 1});
    } else if (language_ == PPIDE_LANG_JSON) {
        addWords({"true", "false", "null"}, constantFmt);
        rules_.push_back({QRegularExpression(R"("([^"\\]|\\.)*"\s*(?=:))"), builtinFmt});
    }

    if (language_ != PPIDE_LANG_MARKDOWN) {
        rules_.push_back({QRegularExpression(R"(\b(?:0[xX][0-9A-Fa-f]+|0[bB][01]+|\d+(?:\.\d+)?(?:[eE][+-]?\d+)?)\b)"), numberFmt_});
    }
}

void SyntaxHighlighter::highlightBlock(const QString &text) {
    if (language_ == PPIDE_LANG_MARKDOWN) {
        const int previous = previousBlockState();
        const bool fence = text.trimmed().startsWith("```");
        if (fence) {
            setFormat(0, text.size(), fenceFmt_);
            setCurrentBlockState(previous == 10 ? 0 : 10);
            return;
        }
        if (previous == 10) {
            QTextCharFormat code = makeFormat("#D4D4D4");
            code.setBackground(QColor("#15181E"));
            setFormat(0, text.size(), code);
            setCurrentBlockState(10);
            return;
        }
        setCurrentBlockState(0);
        if (QRegularExpression(R"(^#{1,6}\s)").match(text).hasMatch())
            setFormat(0, text.size(), headingFmt_);
        auto it = QRegularExpression(R"(`[^`]+`)").globalMatch(text);
        while (it.hasNext()) {
            const auto match = it.next();
            setFormat(match.capturedStart(), match.capturedLength(), stringFmt_);
        }
        it = QRegularExpression(R"(\*\*[^*]+\*\*)").globalMatch(text);
        while (it.hasNext()) {
            const auto match = it.next();
            auto bold = makeFormat("#D7DAE0", true);
            setFormat(match.capturedStart(), match.capturedLength(), bold);
        }
        it = QRegularExpression(R"(\[[^\]]+\]\([^)]+\))").globalMatch(text);
        while (it.hasNext()) {
            const auto match = it.next();
            setFormat(match.capturedStart(), match.capturedLength(), typeFmt_);
        }
        auto quote = QRegularExpression(R"(^\s*>\s)").match(text);
        if (quote.hasMatch()) setFormat(quote.capturedStart(), quote.capturedLength(), commentFmt_);
        auto bullet = QRegularExpression(R"(^\s*(?:[-*+] |\d+\. ))").match(text);
        if (bullet.hasMatch()) setFormat(bullet.capturedStart(), bullet.capturedLength(), keywordFmt_);
        return;
    }

    // Semantic/token rules first.
    for (const auto &rule : rules_) {
        auto it = rule.rx.globalMatch(text);
        while (it.hasNext()) {
            const auto match = it.next();
            const int group = rule.captureGroup;
            const int start = group > 0 ? match.capturedStart(group) : match.capturedStart();
            const int length = group > 0 ? match.capturedLength(group) : match.capturedLength();
            if (start >= 0 && length > 0) setFormat(start, length, rule.fmt);
        }
    }

    // Lexical pass. It respects quotes before recognizing comments, which stops
    // `"https://..."` and similar strings from turning green halfway through.
    bool inBlockComment = previousBlockState() == 1;
    int index = 0;
    while (index < text.size()) {
        if (inBlockComment) {
            const int end = text.indexOf("*/", index);
            if (end < 0) {
                setFormat(index, text.size() - index, commentFmt_);
                setCurrentBlockState(1);
                return;
            }
            setFormat(index, end - index + 2, commentFmt_);
            index = end + 2;
            inBlockComment = false;
            continue;
        }

        if ((language_ == PPIDE_LANG_C || language_ == PPIDE_LANG_CPP ||
             language_ == PPIDE_LANG_HEADER) && text.mid(index, 2) == "/*") {
            inBlockComment = true;
            continue;
        }

        if (text.mid(index, 2) == "//") {
            setFormat(index, text.size() - index, commentFmt_);
            break;
        }
        if ((language_ == PPIDE_LANG_PUNPUN || language_ == PPIDE_LANG_PYTHON) &&
            text.at(index) == '#') {
            setFormat(index, text.size() - index, commentFmt_);
            break;
        }

        const QChar ch = text.at(index);
        if (ch == '"' || ch == '\'') {
            const QChar quote = ch;
            int end = index + 1;
            bool escaped = false;
            while (end < text.size()) {
                const QChar current = text.at(end);
                if (escaped) escaped = false;
                else if (current == '\\') escaped = true;
                else if (current == quote) { ++end; break; }
                ++end;
            }
            setFormat(index, end - index, stringFmt_);
            const QString literal = text.mid(index, end - index);
            auto escapes = QRegularExpression(R"(\\(?:[nrt0\\"']|x[0-9A-Fa-f]{2}|u[0-9A-Fa-f]{4}))").globalMatch(literal);
            while (escapes.hasNext()) {
                const auto escape = escapes.next();
                setFormat(index + escape.capturedStart(), escape.capturedLength(), escapeFmt_);
            }
            index = end;
            continue;
        }
        ++index;
    }
    setCurrentBlockState(inBlockComment ? 1 : 0);
}
