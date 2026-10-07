#ifndef ITLS_PATH_REWRITE_H
#define ITLS_PATH_REWRITE_H

#include <stddef.h>
#include <stdint.h>

#define ITLS_PATH_REWRITE_BUFFER_SIZE 128

typedef enum ItlsReplacementModule {
    ITLS_REPLACEMENT_BROWSER,
    ITLS_REPLACEMENT_LIBHTTP,
    ITLS_REPLACEMENT_LIBSSL,
    ITLS_REPLACEMENT_DNS_FIX,
    ITLS_REPLACEMENT_COMMERCE,
    ITLS_REPLACEMENT_MODULE_COUNT
} ItlsReplacementModule;

typedef struct ItlsReplacementState {
    int browser;
    int app_tls;
    int commerce;
} ItlsReplacementState;

ItlsReplacementState itls_build_replacement_state(
    const int available[ITLS_REPLACEMENT_MODULE_COUNT],
    uint32_t firmware_version
);

const char *itls_select_replacement_path(
    const ItlsReplacementState *state,
    const char *path,
    char *rewrite_buffer,
    size_t rewrite_buffer_size
);

int itls_is_diagnostic_module_path(const char *path);

#endif
