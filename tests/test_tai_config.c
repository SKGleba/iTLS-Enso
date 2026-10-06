#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../installer/tai_config.h"

static void assert_config(
    const char *input,
    int enable,
    const char *expected
) {
    unsigned char *output = NULL;
    size_t output_size = 0;
    int result = itls_build_tai_config(
        (const unsigned char *)input,
        strlen(input),
        enable,
        &output,
        &output_size
    );

    assert(result == 0);
    assert(output != NULL);
    assert(output_size == strlen(expected));
    assert(memcmp(output, expected, output_size) == 0);
    assert(output[output_size] == '\0');
    free(output);
}

static void test_enable_prepends_one_canonical_block(void) {
    assert_config(
        "*KERNEL\nur0:tai/other.skprx\n",
        1,
        "# ITLS\n*KERNEL\nur0:tai/itls.skprx\n\n"
        "*KERNEL\nur0:tai/other.skprx\n"
    );
}

static void test_enable_removes_existing_marker_and_plugin_line(void) {
    assert_config(
        "# ITLS\r\n*KERNEL\r\nur0:tai/itls.skprx\r\nur0:tai/other.skprx\r\n",
        1,
        "# ITLS\n*KERNEL\nur0:tai/itls.skprx\n\n"
        "*KERNEL\r\nur0:tai/other.skprx\r\n"
    );
}

static void test_enable_is_idempotent_for_the_canonical_block(void) {
    assert_config(
        "# ITLS\n*KERNEL\nur0:tai/itls.skprx\n\n"
        "*KERNEL\nur0:tai/other.skprx\n",
        1,
        "# ITLS\n*KERNEL\nur0:tai/itls.skprx\n\n"
        "*KERNEL\nur0:tai/other.skprx\n"
    );
}

static void test_enable_removes_legacy_user_plugin_entry(void) {
    assert_config(
        "# ITLS\n*KERNEL\nur0:tai/itls.skprx\n"
        "*ALL\nur0:tai/itls.suprx\n"
        "*main\nux0:tai/plugin.suprx\n",
        1,
        "# ITLS\n*KERNEL\nur0:tai/itls.skprx\n\n"
        "*KERNEL\n*ALL\n*main\nux0:tai/plugin.suprx\n"
    );
}

static void test_disable_removes_itls_lines_and_preserves_other_content(void) {
    assert_config(
        "# ITLS\n*KERNEL\nur0:tai/itls.skprx\n\n*main\nux0:tai/plugin.suprx\n",
        0,
        "*main\nux0:tai/plugin.suprx\n"
    );
}

static void test_disable_handles_final_line_without_newline(void) {
    assert_config(
        "*KERNEL\nur0:tai/itls.skprx",
        0,
        "*KERNEL\n"
    );
}

static void test_invalid_arguments_are_rejected(void) {
    unsigned char *output = NULL;
    size_t output_size = 0;

    assert(
        itls_build_tai_config(NULL, 0, 1, &output, &output_size)
        == ITLS_CONFIG_ERROR_INVALID_ARGUMENT
    );
    assert(
        itls_build_tai_config(
            (const unsigned char *)"",
            0,
            1,
            NULL,
            &output_size
        ) == ITLS_CONFIG_ERROR_INVALID_ARGUMENT
    );
}

int main(void) {
    test_enable_prepends_one_canonical_block();
    test_enable_removes_existing_marker_and_plugin_line();
    test_enable_is_idempotent_for_the_canonical_block();
    test_enable_removes_legacy_user_plugin_entry();
    test_disable_removes_itls_lines_and_preserves_other_content();
    test_disable_handles_final_line_without_newline();
    test_invalid_arguments_are_rejected();
    puts("taiHEN config tests passed");
    return 0;
}
