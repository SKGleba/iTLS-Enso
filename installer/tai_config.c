#include "tai_config.h"

#include <stdlib.h>
#include <string.h>

static const char config_block[] =
    "# ITLS\n"
    "*KERNEL\n"
    "ur0:tai/itls.skprx\n\n";
static const char config_block_crlf[] =
    "# ITLS\r\n"
    "*KERNEL\r\n"
    "ur0:tai/itls.skprx\r\n\r\n";
static const char config_marker[] = "# ITLS";
static const char config_plugin[] = "ur0:tai/itls.skprx";
static const char legacy_config_plugin[] = "ur0:tai/itls.suprx";

static size_t canonical_block_size(
    const unsigned char *input,
    size_t input_size
) {
    size_t lf_size = sizeof(config_block) - 1;
    size_t crlf_size = sizeof(config_block_crlf) - 1;

    if (input_size >= lf_size && memcmp(input, config_block, lf_size) == 0)
        return lf_size;
    if (
        input_size >= crlf_size
        && memcmp(input, config_block_crlf, crlf_size) == 0
    ) {
        return crlf_size;
    }
    return 0;
}

static int line_equals(
    const unsigned char *line,
    size_t line_size,
    const char *expected
) {
    size_t expected_size = strlen(expected);

    while (
        line_size > 0
        && (line[line_size - 1] == '\n' || line[line_size - 1] == '\r')
    ) {
        line_size--;
    }

    return line_size == expected_size
        && memcmp(line, expected, expected_size) == 0;
}

int itls_build_tai_config(
    const unsigned char *input,
    size_t input_size,
    int enable,
    unsigned char **output_data,
    size_t *output_size
) {
    size_t block_size;
    unsigned char *output;
    size_t input_offset = 0;
    size_t output_offset = 0;

    if (input == NULL || output_data == NULL || output_size == NULL)
        return ITLS_CONFIG_ERROR_INVALID_ARGUMENT;

    block_size = enable ? strlen(config_block) : 0;
    output = malloc(input_size + block_size + 1);
    if (output == NULL)
        return ITLS_CONFIG_ERROR_NO_MEMORY;

    if (enable) {
        memcpy(output, config_block, block_size);
        output_offset = block_size;
    }

    while (input_offset < input_size) {
        const unsigned char *line = input + input_offset;
        size_t remaining_size = input_size - input_offset;
        size_t block_to_skip = canonical_block_size(line, remaining_size);
        const unsigned char *newline = memchr(line, '\n', input_size - input_offset);
        size_t line_size = newline == NULL
            ? input_size - input_offset
            : (size_t)(newline - line) + 1;

        if (block_to_skip != 0) {
            input_offset += block_to_skip;
            continue;
        }

        if (
            !line_equals(line, line_size, config_marker)
            && !line_equals(line, line_size, config_plugin)
            && !line_equals(line, line_size, legacy_config_plugin)
        ) {
            memcpy(output + output_offset, line, line_size);
            output_offset += line_size;
        }

        input_offset += line_size;
    }

    output[output_offset] = '\0';
    *output_data = output;
    *output_size = output_offset;
    return 0;
}
