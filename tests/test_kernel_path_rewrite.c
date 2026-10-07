#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../kernel/path_rewrite.h"

static void assert_path(const char *actual, const char *expected) {
    assert(actual != NULL);
    assert(strcmp(actual, expected) == 0);
}

static void assert_rewrite(
    const ItlsReplacementState *state,
    const char *input,
    const char *expected
) {
    char buffer[ITLS_PATH_REWRITE_BUFFER_SIZE];
    const char *actual = itls_select_replacement_path(
        state,
        input,
        buffer,
        sizeof(buffer)
    );
    assert_path(actual, expected);
}

static void assert_unchanged(
    const ItlsReplacementState *state,
    const char *input
) {
    char buffer[ITLS_PATH_REWRITE_BUFFER_SIZE];
    const char *actual = itls_select_replacement_path(
        state,
        input,
        buffer,
        sizeof(buffer)
    );
    assert(actual == input);
}

static void test_replacement_state_respects_firmware_and_complete_pairs(void) {
    int all_present[ITLS_REPLACEMENT_MODULE_COUNT] = {1, 1, 1, 1, 1};
    int missing_pairs[ITLS_REPLACEMENT_MODULE_COUNT] = {1, 1, 0, 1, 0};
    int commerce_without_app_tls[ITLS_REPLACEMENT_MODULE_COUNT] = {
        1, 1, 0, 1, 1
    };
    ItlsReplacementState state;

    state = itls_build_replacement_state(all_present, 0x03600000);
    assert(state.browser == 1);
    assert(state.app_tls == 1);
    assert(state.commerce == 1);

    state = itls_build_replacement_state(all_present, 0x03650000);
    assert(state.browser == 1);
    assert(state.app_tls == 1);
    assert(state.commerce == 0);

    state = itls_build_replacement_state(missing_pairs, 0x03600000);
    assert(state.browser == 1);
    assert(state.app_tls == 0);
    assert(state.commerce == 0);

    state = itls_build_replacement_state(
        commerce_without_app_tls,
        0x03600000
    );
    assert(state.browser == 1);
    assert(state.app_tls == 0);
    assert(state.commerce == 0);

    state = itls_build_replacement_state(NULL, 0x03600000);
    assert(state.browser == 0);
    assert(state.app_tls == 0);
    assert(state.commerce == 0);
}

static void test_diagnostic_path_filter_tracks_tls_module_loads(void) {
    assert(itls_is_diagnostic_module_path(
        "vs0:data/external/webcore/ScePsp2Compat.suprx"
    ));
    assert(itls_is_diagnostic_module_path(
        "sdbb39dc1e2c4f:/webcore/ScePsp2Compat.suprx"
    ));
    assert(itls_is_diagnostic_module_path(
        "vs0:/data/external/itls/ScePsp2Compat.suprx"
    ));
    assert(itls_is_diagnostic_module_path("app0:/sce_module/libhttp.suprx"));
    assert(itls_is_diagnostic_module_path("vs0:sys/external/libssl.suprx"));
    assert(itls_is_diagnostic_module_path(
        "vs0:/sys/external/np_commerce2.suprx"
    ));
    assert(itls_is_diagnostic_module_path("old_dns_fix.suprx"));
    assert(!itls_is_diagnostic_module_path("vs0:sys/external/libnet.suprx"));
    assert(!itls_is_diagnostic_module_path("libssl.suprx.backup"));
    assert(!itls_is_diagnostic_module_path("customlibssl.suprx"));
    assert(!itls_is_diagnostic_module_path(""));
    assert(!itls_is_diagnostic_module_path(NULL));
}

static void test_browser_path_rewrite_is_bounded_and_non_destructive(void) {
    const ItlsReplacementState all_enabled = {1, 1, 1};
    const ItlsReplacementState all_disabled = {0, 0, 0};
    char observed[] = "sddbb10bfa5365:/webcore/ScePsp2Compat.suprx";
    const char original[] = "sddbb10bfa5365:/webcore/ScePsp2Compat.suprx";
    char buffer[ITLS_PATH_REWRITE_BUFFER_SIZE];
    char short_buffer[8];
    const char *selected;

    selected = itls_select_replacement_path(
        &all_enabled,
        observed,
        buffer,
        sizeof(buffer)
    );
    assert(selected == buffer);
    assert_path(selected, "sddbb10bfa5365:/itls/ScePsp2Compat.suprx");
    assert_path(observed, original);

    assert_rewrite(
        &all_enabled,
        "sdbb39dc1e2c4f:/webcore/ScePsp2Compat.suprx",
        "sdbb39dc1e2c4f:/itls/ScePsp2Compat.suprx"
    );
    assert_rewrite(
        &all_enabled,
        "opaque-mount:/nested/webcore/ScePsp2Compat.suprx",
        "opaque-mount:/nested/itls/ScePsp2Compat.suprx"
    );

    selected = itls_select_replacement_path(
        &all_enabled,
        observed,
        short_buffer,
        sizeof(short_buffer)
    );
    assert(selected == observed);

    selected = itls_select_replacement_path(
        &all_enabled,
        observed,
        NULL,
        0
    );
    assert(selected == observed);

    assert_unchanged(&all_disabled, observed);
    assert_unchanged(
        &all_enabled,
        "/webcore/ScePsp2Compat.suprx"
    );
    assert_unchanged(
        &all_enabled,
        ":/webcore/ScePsp2Compat.suprx"
    );
    assert_unchanged(
        &all_enabled,
        "opaque-mount:/other/ScePsp2Compat.suprx"
    );
    assert_unchanged(
        &all_enabled,
        "opaque-mount:/webcore/ScePsp2Compat.suprx.extra"
    );
}

int main(void) {
    const ItlsReplacementState all_enabled = {1, 1, 1};
    const ItlsReplacementState all_disabled = {0, 0, 0};
    const char *unrelated = "vs0:sys/external/unrelated.suprx";
    const char *short_path = "x";
    char buffer[ITLS_PATH_REWRITE_BUFFER_SIZE];

    test_replacement_state_respects_firmware_and_complete_pairs();
    test_diagnostic_path_filter_tracks_tls_module_loads();
    test_browser_path_rewrite_is_bounded_and_non_destructive();

    assert_rewrite(
        &all_enabled,
        "vs0:data/external/webcore/ScePsp2Compat.suprx",
        "vs0:data/external/itls/ScePsp2Compat.suprx"
    );
    assert_rewrite(
        &all_enabled,
        "vs0:/data/external/webcore/ScePsp2Compat.suprx",
        "vs0:/data/external/itls/ScePsp2Compat.suprx"
    );
    assert_rewrite(
        &all_enabled,
        "vs0:sys/external/libhttp.suprx",
        "vs0:/data/external/itls/libhttp.suprx"
    );
    assert_rewrite(
        &all_enabled,
        "vs0:/sys/external/libhttp.suprx",
        "vs0:/data/external/itls/libhttp.suprx"
    );
    assert_rewrite(
        &all_enabled,
        "vs0:sys/external/libssl.suprx",
        "vs0:/data/external/itls/libssl.suprx"
    );
    assert_rewrite(
        &all_enabled,
        "vs0:/sys/external/libssl.suprx",
        "vs0:/data/external/itls/libssl.suprx"
    );
    assert_rewrite(
        &all_enabled,
        "vs0:sys/external/np_commerce2.suprx",
        "vs0:/data/external/itls/np_commerce2.suprx"
    );
    assert_rewrite(
        &all_enabled,
        "vs0:/sys/external/np_commerce2.suprx",
        "vs0:/data/external/itls/np_commerce2.suprx"
    );

    assert_unchanged(&all_enabled, unrelated);
    assert_unchanged(&all_enabled, short_path);
    assert_unchanged(
        &all_enabled,
        "vs0:sys/external/libhttp.suprx.extra"
    );
    assert_unchanged(
        &all_enabled,
        "vs0:data/external/notwebcore/ScePsp2Compat.suprx"
    );
    assert_rewrite(
        &all_enabled,
        "vs0:other/webcore/ScePsp2Compat.suprx",
        "vs0:other/itls/ScePsp2Compat.suprx"
    );
    assert_rewrite(
        &all_enabled,
        "ux0:data/external/webcore/ScePsp2Compat.suprx",
        "ux0:data/external/itls/ScePsp2Compat.suprx"
    );
    assert_unchanged(
        &all_disabled,
        "vs0:data/external/webcore/ScePsp2Compat.suprx"
    );

    assert(
        itls_select_replacement_path(
            NULL,
            unrelated,
            buffer,
            sizeof(buffer)
        ) == unrelated
    );
    assert(
        itls_select_replacement_path(
            &all_enabled,
            NULL,
            buffer,
            sizeof(buffer)
        ) == NULL
    );

    puts("kernel path rewrite tests passed");
    return 0;
}
