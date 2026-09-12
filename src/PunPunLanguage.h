#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

/// The PunPun vocabulary, in one place.
///
/// The highlighter, the completion list and the offline analyzer used to carry
/// three private copies of "what PunPun looks like", and all three had drifted
/// from the compiler: they painted `trait`/`impl`, which the grammar does not
/// have, offered `bring` as the way to import, and flagged the modern `import`
/// as a mistake. Every table below is transcribed from the compiler's own
/// keyword macro (compiler/include/ppc/syntax/token.hpp) and builtin table
/// (compiler/src/sema/builtins.cpp), so there is a single place to update when
/// the language grows.
namespace PunPunLanguage {

/// Declaration keywords: these introduce something with a name.
inline QStringList declarationKeywords() {
    return {"fn",     "struct", "object", "sealed",  "init",     "enum",
            "contract", "meets", "import", "extern", "native",   "inject",
            "let",    "mut",    "const",  "public",  "private",  "protected"};
}

/// Control flow and expression keywords.
inline QStringList flowKeywords() {
    return {"if",     "else",   "while",  "for",    "in",     "match",
            "case",   "where",  "return", "break",  "continue", "async",
            "await",  "launch", "move",   "unsafe", "raw",    "sizeof",
            "alignof", "self",  "as",     "and",    "or",     "not"};
}

/// Built-in type spellings. Not keywords -- they are named types -- but the
/// compiler still reserves them, so they are painted and offered as types.
/// Exactly the spellings `Checker::resolve_type` accepts, plus the two prelude
/// enums. `Task` and `Bytes` are deliberately absent: the type is spelled
/// `bytes`, and tasks are produced by `async`, not named.
inline QStringList typeNames() {
    return {"int",  "i64",  "i32",   "u64",  "u32",   "float",  "f64",
            "f32",  "bool", "str",   "text", "String", "nums",  "bytes",
            "void", "List", "Map",   "Slice", "Option", "Result"};
}

inline QStringList constants() {
    return {"true", "false"};
}

/// The migration dialect. These still parse so that 0.6 source builds, but the
/// compiler answers each one with warning W2000 naming the modern spelling.
struct LegacyForm {
    const char *legacy;
    const char *modern;
    const char *advice;
    /// `each`, `give`, `next` and friends are ordinary English words as well as
    /// migration keywords. Those are reported only where a statement can begin,
    /// so a variable named `next` or a call to `give(...)` stays quiet. The
    /// distinctive spellings are reported wherever they appear.
    bool statementOnly = false;
};

inline QVector<LegacyForm> legacyForms() {
    return {
        {"craft", "fn", "Declare functions with `fn`.", false},
        {"shape", "struct", "Declare value types with `struct`.", false},
        {"bring", "import", "Import modules with `import std.io`.", false},
        {"whilst", "while", "Write loops with `while`.", false},
        {"otherwise", "else", "Write the alternative branch with `else`.", false},
        {"gives", "->", "Write the result type as `fn name() -> T`.", false},
        {"pin", "let", "Bind an immutable value with `let`.", true},
        {"keep", "let mut", "Bind a mutable value with `let mut`.", true},
        {"when", "if", "Write conditionals with `if`.", true},
        {"each", "for ... in", "Write iteration as `for name in sequence`.", true},
        {"give", "return", "Return a value with `return`.", true},
        {"leave", "break", "Leave a loop with `break`.", true},
        {"next", "continue", "Skip to the next iteration with `continue`.", true},
        {"yes", "true", "Spell the boolean literal `true`.", true},
        {"no", "false", "Spell the boolean literal `false`.", true},
    };
}

/// Standard-library calls available without importing anything, taken from the
/// compiler's builtin table. Internal `pp_*` runtime symbols are excluded:
/// they are not callable from source.
inline QStringList builtins() {
    return {
        "abs", "acos", "append_text", "arg", "arg_count", "asin", "assert", "at",
        "atan", "atan2", "bytes", "bytes_at", "bytes_concat", "bytes_from_text",
        "bytes_len", "bytes_push", "bytes_put", "bytes_slice", "bytes_to_text",
        "cancel", "cancelled", "ceil", "char_at", "char_str", "clock_ms", "concat",
        "contains", "cos", "current_dir", "day_of", "decimal", "drop", "ends_with",
        "env_has", "env_or", "exit", "exp", "fabs", "file_exists", "file_size",
        "floor", "fmod", "gui_available", "gui_message", "hour_of",
        "https_available", "https_error", "https_request", "https_status", "hypot",
        "index_of", "is_dir", "is_infinite", "is_nan", "join", "last_index_of",
        "len", "list", "list_at", "list_clear", "list_dir", "list_pop",
        "list_push", "list_put", "list_size", "log", "log10", "log2", "make_dir",
        "make_dirs", "map", "map_clear", "map_get", "map_get_or", "map_has",
        "map_keys", "map_put", "map_remove", "map_size", "minute_of", "month_of",
        "move", "now_ms", "numbers", "pad_left", "pad_right", "panic",
        "parse_float", "parse_int", "path_extension", "path_join", "path_name",
        "path_parent", "platform", "pop", "pow", "print", "println", "push", "put",
        "random_bytes", "random_float", "random_int", "random_seed", "read_bytes",
        "read_line", "read_text", "remove_dir", "remove_file", "rename_file",
        "repeat", "replace", "round", "run_command", "say", "second_of", "sin",
        "size", "sleep_ms", "slice", "slice_get", "slice_len", "sort", "split",
        "sqrt", "starts_with", "tan", "task_done", "task_group", "task_group_add",
        "task_group_cancel", "task_group_close", "task_group_done",
        "task_group_pending", "task_group_wait", "task_group_wait_for", "text",
        "text_float", "to_lower", "to_upper", "trim", "utf8_len", "utf8_valid",
        "view", "weekday_of", "whole", "write_bytes", "write_text", "year_of"};
}

/// Everything a completion popup should offer in a .pp file.
inline QStringList completionWords() {
    QStringList words = declarationKeywords();
    words << flowKeywords() << typeNames() << constants() << builtins();
    words.removeDuplicates();
    words.sort(Qt::CaseInsensitive);
    return words;
}

}  // namespace PunPunLanguage
