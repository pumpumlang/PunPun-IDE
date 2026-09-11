#ifndef PUNPUN_PLUGIN_API_H
#define PUNPUN_PLUGIN_API_H

#ifdef __cplusplus
extern "C" {
#endif

#define PPIDE_PLUGIN_ABI_VERSION 1

typedef enum ppide_language {
    PPIDE_LANG_TEXT = 0,
    PPIDE_LANG_PUNPUN,
    PPIDE_LANG_C,
    PPIDE_LANG_CPP,
    PPIDE_LANG_HEADER,
    PPIDE_LANG_MARKDOWN,
    PPIDE_LANG_JSON,
    PPIDE_LANG_JAVASCRIPT,
    PPIDE_LANG_PYTHON
} ppide_language;

int ppide_plugin_abi_version(void);
ppide_language ppide_detect_language(const char *path);
const char *ppide_language_name(ppide_language language);

#ifdef __cplusplus
}
#endif

#endif
