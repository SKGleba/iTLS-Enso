#ifndef ITLS_TAI_CONFIG_H
#define ITLS_TAI_CONFIG_H

#include <stddef.h>

#define ITLS_CONFIG_ERROR_INVALID_ARGUMENT (-0x7201)
#define ITLS_CONFIG_ERROR_NO_MEMORY (-0x7202)

int itls_build_tai_config(
    const unsigned char *input,
    size_t input_size,
    int enable,
    unsigned char **output_data,
    size_t *output_size
);

#endif
