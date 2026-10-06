#include "path_rewrite.h"

#include <stddef.h>
#include <string.h>

static const char browser_suffix[] = "webcore/ScePsp2Compat.suprx";
static const char browser_replacement[] =
    "vs0:/data/external/itls/ScePsp2Compat.suprx";
static const char libhttp_path[] = "vs0:sys/external/libhttp.suprx";
static const char libhttp_replacement[] =
    "vs0:/data/external/itls/libhttp.suprx";
static const char libssl_path[] = "vs0:sys/external/libssl.suprx";
static const char libssl_replacement[] =
    "vs0:/data/external/itls/libssl.suprx";
static const char commerce_path[] = "vs0:sys/external/np_commerce2.suprx";
static const char commerce_replacement[] =
    "vs0:/data/external/itls/np_commerce2.suprx";

const char *itls_select_replacement_path(
    const ItlsReplacementState *state,
    const char *path
) {
    size_t path_length;
    size_t suffix_length;

    if (state == NULL || path == NULL || path[0] == '\0') {
        return path;
    }

    if (state->app_tls) {
        if (strcmp(path, libhttp_path) == 0) {
            return libhttp_replacement;
        }
        if (strcmp(path, libssl_path) == 0) {
            return libssl_replacement;
        }
    }

    if (state->browser) {
        path_length = strlen(path);
        suffix_length = sizeof(browser_suffix) - 1;
        if (
            path_length >= suffix_length
            && strcmp(path + path_length - suffix_length, browser_suffix) == 0
        ) {
            return browser_replacement;
        }
    }

    if (state->commerce && strcmp(path, commerce_path) == 0) {
        return commerce_replacement;
    }

    return path;
}
