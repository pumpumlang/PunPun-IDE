#include "punpun_plugin_api.h"
#include <ctype.h>
#include <stddef.h>
#include <string.h>

static int equals_ci(const char *a, const char *b) {
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
        ++a; ++b;
    }
    return *a == *b;
}

int ppide_plugin_abi_version(void) { return PPIDE_PLUGIN_ABI_VERSION; }

ppide_language ppide_detect_language(const char *path) {
    if (!path) return PPIDE_LANG_TEXT;
    const char *dot = strrchr(path, '.');
    if (!dot) return PPIDE_LANG_TEXT;
    if (equals_ci(dot, ".pp")) return PPIDE_LANG_PUNPUN;
    if (equals_ci(dot, ".c")) return PPIDE_LANG_C;
    if (equals_ci(dot, ".cc") || equals_ci(dot, ".cpp") || equals_ci(dot, ".cxx")) return PPIDE_LANG_CPP;
    if (equals_ci(dot, ".h") || equals_ci(dot, ".hh") || equals_ci(dot, ".hpp") || equals_ci(dot, ".hxx")) return PPIDE_LANG_HEADER;
    if (equals_ci(dot, ".md") || equals_ci(dot, ".markdown")) return PPIDE_LANG_MARKDOWN;
    if (equals_ci(dot, ".json")) return PPIDE_LANG_JSON;
    if (equals_ci(dot, ".js") || equals_ci(dot, ".mjs") || equals_ci(dot, ".cjs")) return PPIDE_LANG_JAVASCRIPT;
    if (equals_ci(dot, ".py")) return PPIDE_LANG_PYTHON;
    return PPIDE_LANG_TEXT;
}

const char *ppide_language_name(ppide_language language) {
    switch (language) {
        case PPIDE_LANG_PUNPUN: return "PunPun";
        case PPIDE_LANG_C: return "C";
        case PPIDE_LANG_CPP: return "C++";
        case PPIDE_LANG_HEADER: return "C/C++ Header";
        case PPIDE_LANG_MARKDOWN: return "Markdown";
        case PPIDE_LANG_JSON: return "JSON";
        case PPIDE_LANG_JAVASCRIPT: return "JavaScript";
        case PPIDE_LANG_PYTHON: return "Python";
        default: return "Plain Text";
    }
}
