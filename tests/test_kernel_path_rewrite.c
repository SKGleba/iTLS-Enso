#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../kernel/path_rewrite.h"

static void assert_path(const char *actual, const char *expected) {
    assert(actual != NULL);
    assert(strcmp(actual, expected) == 0);
}

int main(void) {
    const ItlsReplacementState all_enabled = {1, 1, 1};
    const ItlsReplacementState all_disabled = {0, 0, 0};
    const char *unrelated = "vs0:sys/external/unrelated.suprx";
    const char *short_path = "x";

    assert_path(
        itls_select_replacement_path(
            &all_enabled,
            "vs0:data/external/webcore/ScePsp2Compat.suprx"
        ),
        "vs0:/data/external/itls/ScePsp2Compat.suprx"
    );
    assert_path(
        itls_select_replacement_path(
            &all_enabled,
            "vs0:/data/external/webcore/ScePsp2Compat.suprx"
        ),
        "vs0:/data/external/itls/ScePsp2Compat.suprx"
    );
    assert_path(
        itls_select_replacement_path(&all_enabled, "vs0:sys/external/libhttp.suprx"),
        "vs0:/data/external/itls/libhttp.suprx"
    );
    assert_path(
        itls_select_replacement_path(&all_enabled, "vs0:sys/external/libssl.suprx"),
        "vs0:/data/external/itls/libssl.suprx"
    );
    assert_path(
        itls_select_replacement_path(
            &all_enabled,
            "vs0:sys/external/np_commerce2.suprx"
        ),
        "vs0:/data/external/itls/np_commerce2.suprx"
    );
    assert(itls_select_replacement_path(&all_enabled, unrelated) == unrelated);
    assert(itls_select_replacement_path(&all_enabled, short_path) == short_path);
    assert_path(
        itls_select_replacement_path(
            &all_enabled,
            "vs0:sys/external/libhttp.suprx.extra"
        ),
        "vs0:sys/external/libhttp.suprx.extra"
    );
    assert_path(
        itls_select_replacement_path(
            &all_disabled,
            "vs0:data/external/webcore/ScePsp2Compat.suprx"
        ),
        "vs0:data/external/webcore/ScePsp2Compat.suprx"
    );
    assert(itls_select_replacement_path(NULL, unrelated) == unrelated);
    assert(itls_select_replacement_path(&all_enabled, NULL) == NULL);

    puts("kernel path rewrite tests passed");
    return 0;
}
