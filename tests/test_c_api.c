#include "punpun_plugin_api.h"
#include <assert.h>
#include <string.h>

int main(void) {
    assert(ppide_plugin_abi_version() == 1);
    assert(ppide_detect_language("hello.pp") == PPIDE_LANG_PUNPUN);
    assert(ppide_detect_language("main.CPP") == PPIDE_LANG_CPP);
    assert(ppide_detect_language("thing.hpp") == PPIDE_LANG_HEADER);
    assert(ppide_detect_language("README.md") == PPIDE_LANG_MARKDOWN);
    assert(strcmp(ppide_language_name(PPIDE_LANG_PUNPUN), "PunPun") == 0);
    return 0;
}
