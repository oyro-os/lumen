#include <lumen/lumen.h>

extern "C" {

const char* lumen_version_string() {
    return LUMEN_VERSION_STRING;
}

int lumen_version_major() {
    return LUMEN_VERSION_MAJOR;
}

int lumen_version_minor() {
    return LUMEN_VERSION_MINOR;
}

int lumen_version_patch() {
    return LUMEN_VERSION_PATCH;
}

} // extern "C"