#ifndef ITLS_PATH_REWRITE_H
#define ITLS_PATH_REWRITE_H

typedef struct ItlsReplacementState {
    int browser;
    int app_tls;
    int commerce;
} ItlsReplacementState;

const char *itls_select_replacement_path(
    const ItlsReplacementState *state,
    const char *path
);

#endif
