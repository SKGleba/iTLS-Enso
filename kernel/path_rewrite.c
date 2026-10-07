#include "path_rewrite.h"

#include <stddef.h>
#include <string.h>

static const char browser_path_suffix[] =
    "/webcore/ScePsp2Compat.suprx";
static const char browser_replacement_suffix[] =
    "/itls/ScePsp2Compat.suprx";
static const char libhttp_path[] = "vs0:sys/external/libhttp.suprx";
static const char libhttp_path_slash[] = "vs0:/sys/external/libhttp.suprx";
static const char libhttp_replacement[] =
    "vs0:/data/external/itls/libhttp.suprx";
static const char libssl_path[] = "vs0:sys/external/libssl.suprx";
static const char libssl_path_slash[] = "vs0:/sys/external/libssl.suprx";
static const char libssl_replacement[] =
    "vs0:/data/external/itls/libssl.suprx";
static const char commerce_path[] = "vs0:sys/external/np_commerce2.suprx";
static const char commerce_path_slash[] =
    "vs0:/sys/external/np_commerce2.suprx";
static const char commerce_replacement[] =
    "vs0:/data/external/itls/np_commerce2.suprx";

static const char *rewrite_browser_path(
    const char *path,
    char *rewrite_buffer,
    size_t rewrite_buffer_size
) {
    const char *mount_delimiter;
    size_t path_length;
    size_t path_suffix_length;
    size_t prefix_length;
    size_t replacement_suffix_size;

    path_length = strlen(path);
    path_suffix_length = sizeof(browser_path_suffix) - 1;
    if (
        path_length < path_suffix_length
        || strcmp(path + path_length - path_suffix_length, browser_path_suffix) != 0
    ) {
        return path;
    }

    prefix_length = path_length - path_suffix_length;
    mount_delimiter = memchr(path, ':', prefix_length);
    if (mount_delimiter == NULL || mount_delimiter == path)
        return path;

    replacement_suffix_size = sizeof(browser_replacement_suffix);
    if (
        rewrite_buffer == NULL
        || prefix_length >= rewrite_buffer_size
        || replacement_suffix_size > rewrite_buffer_size - prefix_length
    ) {
        return path;
    }

    memcpy(rewrite_buffer, path, prefix_length);
    memcpy(
        rewrite_buffer + prefix_length,
        browser_replacement_suffix,
        replacement_suffix_size
    );
    return rewrite_buffer;
}

static int path_has_basename(const char *path, const char *basename) {
    size_t path_length;
    size_t basename_length;
    const char *candidate;

    if (path == NULL || basename == NULL || path[0] == '\0')
        return 0;

    path_length = strlen(path);
    basename_length = strlen(basename);
    if (path_length < basename_length)
        return 0;

    candidate = path + path_length - basename_length;
    if (strcmp(candidate, basename) != 0)
        return 0;

    return candidate == path || candidate[-1] == '/' || candidate[-1] == ':';
}

int itls_is_diagnostic_module_path(const char *path) {
    static const char *const basenames[] = {
        "ScePsp2Compat.suprx",
        "libhttp.suprx",
        "libssl.suprx",
        "old_dns_fix.suprx",
        "np_commerce2.suprx"
    };
    size_t i;

    for (i = 0; i < sizeof(basenames) / sizeof(basenames[0]); i++) {
        if (path_has_basename(path, basenames[i]))
            return 1;
    }

    return 0;
}

ItlsReplacementState itls_build_replacement_state(
    const int available[ITLS_REPLACEMENT_MODULE_COUNT],
    uint32_t firmware_version
) {
    ItlsReplacementState state = {0, 0, 0};

    if (available == NULL)
        return state;

    state.browser = available[ITLS_REPLACEMENT_BROWSER] != 0;
    state.app_tls = available[ITLS_REPLACEMENT_LIBHTTP] != 0
        && available[ITLS_REPLACEMENT_LIBSSL] != 0;
    state.commerce = state.app_tls
        && firmware_version == 0x03600000
        && available[ITLS_REPLACEMENT_DNS_FIX] != 0
        && available[ITLS_REPLACEMENT_COMMERCE] != 0;
    return state;
}

const char *itls_select_replacement_path(
    const ItlsReplacementState *state,
    const char *path,
    char *rewrite_buffer,
    size_t rewrite_buffer_size
) {
    if (state == NULL || path == NULL || path[0] == '\0')
        return path;

    if (state->app_tls) {
        if (
            strcmp(path, libhttp_path) == 0
            || strcmp(path, libhttp_path_slash) == 0
        ) {
            return libhttp_replacement;
        }
        if (
            strcmp(path, libssl_path) == 0
            || strcmp(path, libssl_path_slash) == 0
        ) {
            return libssl_replacement;
        }
    }

    if (state->browser) {
        const char *browser_path = rewrite_browser_path(
            path,
            rewrite_buffer,
            rewrite_buffer_size
        );
        if (browser_path != path)
            return browser_path;
    }

    if (state->commerce) {
        if (
            strcmp(path, commerce_path) == 0
            || strcmp(path, commerce_path_slash) == 0
        ) {
            return commerce_replacement;
        }
    }

    return path;
}
