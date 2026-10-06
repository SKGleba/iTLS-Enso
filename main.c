/*
	iTLS-Enso v3.3 by SKGleba
	All Rights Reserved
*/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <psp2/ctrl.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/power.h>
#include <psp2/vshbridge.h>

#include "graphics.h"
#include "installer/ca_bundle.h"
#include "installer/file_transaction.h"
#include "installer/tai_config.h"

#define printf psvDebugScreenPrintf

#define ITLS_ERROR_NO_MEMORY (-0x7101)
#define ITLS_ERROR_SHORT_READ (-0x7102)
#define ITLS_ERROR_SHORT_WRITE (-0x7103)
#define ITLS_ERROR_PATH_TOO_LONG (-0x7104)
#define ITLS_ERROR_NOT_DIRECTORY (-0x7105)
#define ITLS_ERROR_INVALID_CONFIG (-0x7106)
#define ITLS_ERROR_NOT_FOUND ((int32_t)0x80010002u)

#define ITLS_DIRECTORY "vs0:/data/external/itls"
#define TAI_CONFIG_PATH "ur0:tai/config.txt"
#define TAI_CONFIG_STAGE "ur0:tai/config.txt.itls-stage"
#define TAI_CONFIG_TEMP "ur0:tai/config.txt.itls-new"
#define TAI_CONFIG_ROLLBACK "ur0:tai/config.txt.itls-rollback"
#define KERNEL_PLUGIN_PATH "ur0:tai/itls.skprx"
#define BROWSER_MODULE_PATH ITLS_DIRECTORY "/ScePsp2Compat.suprx"
#define LIBHTTP_MODULE_PATH ITLS_DIRECTORY "/libhttp.suprx"
#define LIBSSL_MODULE_PATH ITLS_DIRECTORY "/libssl.suprx"
#define DNS_FIX_MODULE_PATH ITLS_DIRECTORY "/old_dns_fix.suprx"
#define COMMERCE_MODULE_PATH ITLS_DIRECTORY "/np_commerce2.suprx"
#define LEGACY_BROWSER_MODULE_PATH \
    "vs0:/data/external/webcore/ScePsp2Compat.supr0"
#define LEGACY_LIBHTTP_MODULE_PATH "vs0:sys/external/libhttp.supr0"
#define CA_ACTIVE_PATH "vs0:/data/external/cert/CA_LIST.cer"
#define CA_BACKUP_PATH "vs0:/data/external/cert/CA_LIST.cer_old"
#define CA_TEMP_PATH "vs0:/data/external/cert/CA_LIST.cer.itls-new"
#define CA_ROLLBACK_PATH "vs0:/data/external/cert/CA_LIST.cer.itls-rollback"

#define APP_KERNEL_PLUGIN "app0:kernel.skprx"
#define APP_BROWSER_MODULE "app0:compat.suprx"
#define APP_LIBHTTP_MODULE "app0:lhttp.suprx"
#define APP_LIBSSL_MODULE "app0:lssl.suprx"
#define APP_DNS_FIX_MODULE "app0:dnsfix.suprx"
#define APP_COMMERCE_MODULE "app0:comm2.suprx"
#define APP_CA_BUNDLE "app0:certs.cer"

static const char *last_stage = "idle";
static int last_warning = 0;

typedef enum MenuAction {
    ACTION_INSTALL_FULL,
    ACTION_INSTALL_PLUGIN,
    ACTION_INSTALL_BROWSER,
    ACTION_INSTALL_APP_TLS,
    ACTION_INSTALL_CERTIFICATES,
    ACTION_REMOVE_PLUGIN,
    ACTION_REMOVE_BROWSER,
    ACTION_REMOVE_APP_TLS,
    ACTION_RESTORE_CERTIFICATES,
    ACTION_REMOVE_FULL,
    ACTION_EXIT,
    ACTION_COUNT
} MenuAction;

static const char *const menu_items[ACTION_COUNT] = {
    " -> Install/update the full iTLS package",
    " -> Install/update the iTLS kernel plugin",
    " -> Install/update the browser compatibility module",
    " -> Install/update the in-app HTTP/SSL modules",
    " -> Install/update the CA certificate bundle",
    " -> Remove the iTLS kernel plugin",
    " -> Remove the browser compatibility module",
    " -> Remove the in-app HTTP/SSL modules",
    " -> Restore the original CA certificate bundle",
    " -> Remove the full iTLS package",
    " -> Exit"
};

static int selected_action = 0;

static void set_stage(const char *stage) {
    last_stage = stage;
}

static void record_warning(int error) {
    if (last_warning == 0 && error < 0)
        last_warning = error;
}

static int file_exists(void *context, const char *path) {
    SceIoStat stat;
    int result;
    (void)context;

    memset(&stat, 0, sizeof(stat));
    result = sceIoGetstat(path, &stat);
    if (result >= 0)
        return 1;
    if (result == ITLS_ERROR_NOT_FOUND)
        return 0;
    return result;
}

static int remove_file(void *context, const char *path) {
    (void)context;
    return sceIoRemove(path);
}

static int rename_file(void *context, const char *from, const char *to) {
    (void)context;
    return sceIoRename(from, to);
}

static int close_file(SceUID descriptor, int current_error) {
    int close_result;

    if (descriptor < 0)
        return current_error;

    close_result = sceIoClose(descriptor);
    if (current_error == 0 && close_result < 0)
        return close_result;
    return current_error;
}

static int copy_file_checked(const char *source, const char *destination) {
    enum { COPY_BUFFER_SIZE = 64 * 1024 };
    unsigned char *buffer = NULL;
    SceUID input = -1;
    SceUID output = -1;
    int result = 0;

    buffer = malloc(COPY_BUFFER_SIZE);
    if (buffer == NULL)
        return ITLS_ERROR_NO_MEMORY;

    input = sceIoOpen(source, SCE_O_RDONLY, 0);
    if (input < 0) {
        result = input;
        goto cleanup;
    }

    output = sceIoOpen(
        destination,
        SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC,
        0666
    );
    if (output < 0) {
        result = output;
        goto cleanup;
    }

    for (;;) {
        SceSSize read_size = sceIoRead(input, buffer, COPY_BUFFER_SIZE);
        SceSSize written = 0;

        if (read_size < 0) {
            result = (int)read_size;
            break;
        }
        if (read_size == 0)
            break;

        while (written < read_size) {
            SceSSize write_size = sceIoWrite(
                output,
                buffer + written,
                (SceSize)(read_size - written)
            );
            if (write_size < 0) {
                result = (int)write_size;
                break;
            }
            if (write_size == 0) {
                result = ITLS_ERROR_SHORT_WRITE;
                break;
            }
            written += write_size;
        }

        if (result < 0)
            break;
    }

    if (result == 0) {
        int sync_result = sceIoSyncByFd(output, 0);
        if (sync_result < 0)
            result = sync_result;
    }

cleanup:
    result = close_file(output, result);
    result = close_file(input, result);
    free(buffer);

    if (result < 0)
        sceIoRemove(destination);
    return result;
}

static int copy_file(void *context, const char *source, const char *destination) {
    (void)context;
    return copy_file_checked(source, destination);
}

static int same_file_size(void *context, const char *first, const char *second) {
    SceIoStat first_stat;
    SceIoStat second_stat;
    int result;
    (void)context;

    memset(&first_stat, 0, sizeof(first_stat));
    memset(&second_stat, 0, sizeof(second_stat));

    result = sceIoGetstat(first, &first_stat);
    if (result < 0)
        return result;
    result = sceIoGetstat(second, &second_stat);
    if (result < 0)
        return result;

    return first_stat.st_size == second_stat.st_size;
}

static const ItlsFileOps file_operations = {
    NULL,
    file_exists,
    remove_file,
    rename_file,
    copy_file,
    same_file_size,
};

static int remove_if_present(const char *path) {
    int exists = file_exists(NULL, path);
    if (exists < 0)
        return exists;
    if (!exists)
        return 0;
    return sceIoRemove(path);
}

static int write_buffer_checked(
    const char *path,
    const unsigned char *data,
    size_t size
) {
    SceUID output = -1;
    size_t offset = 0;
    int result = 0;

    output = sceIoOpen(path, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0666);
    if (output < 0)
        return output;

    while (offset < size) {
        SceSSize written = sceIoWrite(
            output,
            data + offset,
            (SceSize)(size - offset)
        );
        if (written < 0) {
            result = (int)written;
            break;
        }
        if (written == 0) {
            result = ITLS_ERROR_SHORT_WRITE;
            break;
        }
        offset += (size_t)written;
    }

    if (result == 0) {
        int sync_result = sceIoSyncByFd(output, 0);
        if (sync_result < 0)
            result = sync_result;
    }

    result = close_file(output, result);
    if (result < 0)
        sceIoRemove(path);
    return result;
}

static int read_file_checked(
    const char *path,
    unsigned char **output_data,
    size_t *output_size
) {
    SceIoStat stat;
    unsigned char *data = NULL;
    SceUID input = -1;
    size_t expected_size;
    size_t offset = 0;
    int result = 0;

    if (output_data == NULL || output_size == NULL)
        return ITLS_ERROR_INVALID_CONFIG;

    memset(&stat, 0, sizeof(stat));
    result = sceIoGetstat(path, &stat);
    if (result < 0)
        return result;
    if (stat.st_size < 0 || (uint64_t)stat.st_size >= SIZE_MAX)
        return ITLS_ERROR_INVALID_CONFIG;

    expected_size = (size_t)stat.st_size;
    data = malloc(expected_size + 1);
    if (data == NULL)
        return ITLS_ERROR_NO_MEMORY;

    input = sceIoOpen(path, SCE_O_RDONLY, 0);
    if (input < 0) {
        result = input;
        goto cleanup;
    }

    while (offset < expected_size) {
        SceSSize read_size = sceIoRead(
            input,
            data + offset,
            (SceSize)(expected_size - offset)
        );
        if (read_size < 0) {
            result = (int)read_size;
            goto cleanup;
        }
        if (read_size == 0) {
            result = ITLS_ERROR_SHORT_READ;
            goto cleanup;
        }
        offset += (size_t)read_size;
    }

    data[expected_size] = '\0';
    *output_data = data;
    *output_size = expected_size;
    data = NULL;

cleanup:
    result = close_file(input, result);
    free(data);
    return result;
}

static int apply_replace_result(ItlsReplaceResult result) {
    if (result.cleanup_error < 0)
        record_warning(result.cleanup_error);
    return result.error;
}

static int update_tai_config(int enable) {
    unsigned char *input = NULL;
    unsigned char *output = NULL;
    size_t input_size = 0;
    size_t output_size = 0;
    ItlsReplaceResult replace_result;
    int result;
    int cleanup_result;

    set_stage(enable ? "enable kernel plugin in taiHEN config" : "remove kernel plugin from taiHEN config");

    result = read_file_checked(TAI_CONFIG_PATH, &input, &input_size);
    if (result < 0)
        goto cleanup;

    result = itls_build_tai_config(
        input,
        input_size,
        enable,
        &output,
        &output_size
    );
    if (result < 0)
        goto cleanup;

    result = remove_if_present(TAI_CONFIG_STAGE);
    if (result < 0)
        goto cleanup;

    result = write_buffer_checked(TAI_CONFIG_STAGE, output, output_size);
    if (result < 0)
        goto cleanup;

    replace_result = itls_transactional_replace(
        &file_operations,
        TAI_CONFIG_STAGE,
        TAI_CONFIG_PATH,
        TAI_CONFIG_TEMP,
        TAI_CONFIG_ROLLBACK
    );
    result = apply_replace_result(replace_result);

cleanup:
    cleanup_result = remove_if_present(TAI_CONFIG_STAGE);
    if (cleanup_result < 0)
        record_warning(cleanup_result);
    free(output);
    free(input);
    return result;
}

static int make_side_path(
    char *output,
    size_t output_size,
    const char *path,
    const char *suffix
) {
    int written = snprintf(output, output_size, "%s%s", path, suffix);
    if (written < 0 || (size_t)written >= output_size)
        return ITLS_ERROR_PATH_TOO_LONG;
    return 0;
}

static int replace_payload(
    const char *stage,
    const char *source,
    const char *destination
) {
    char temporary[256];
    char rollback[256];
    ItlsReplaceResult replace_result;
    int result;

    set_stage(stage);
    result = make_side_path(temporary, sizeof(temporary), destination, ".itls-new");
    if (result < 0)
        return result;
    result = make_side_path(rollback, sizeof(rollback), destination, ".itls-rollback");
    if (result < 0)
        return result;

    replace_result = itls_transactional_replace(
        &file_operations,
        source,
        destination,
        temporary,
        rollback
    );
    return apply_replace_result(replace_result);
}

static int remove_payload(const char *stage, const char *destination) {
    set_stage(stage);
    return remove_if_present(destination);
}

static int ensure_directory(const char *path) {
    SceIoStat stat;
    int result;

    memset(&stat, 0, sizeof(stat));
    result = sceIoGetstat(path, &stat);
    if (result >= 0)
        return SCE_S_ISDIR(stat.st_mode) ? 0 : ITLS_ERROR_NOT_DIRECTORY;
    if (result != ITLS_ERROR_NOT_FOUND)
        return result;

    result = sceIoMkdir(path, 0777);
    if (result < 0) {
        memset(&stat, 0, sizeof(stat));
        if (sceIoGetstat(path, &stat) >= 0 && SCE_S_ISDIR(stat.st_mode))
            return 0;
        return result;
    }
    return 0;
}

static int mount_vs0_rw(void) {
    unsigned char mount_buffer[0x100];
    int result;

    memset(mount_buffer, 0, sizeof(mount_buffer));
    vshIoUmount(0x300, 0, 0, 0);
    vshIoUmount(0x300, 1, 0, 0);

    result = _vshIoMount(0x300, 0, 2, mount_buffer);
    if (result < 0)
        return result;

    return ensure_directory(ITLS_DIRECTORY);
}

static int get_firmware_version(uint32_t *version) {
    SceKernelFwInfo firmware;
    int result;

    if (version == NULL)
        return ITLS_ERROR_INVALID_CONFIG;

    memset(&firmware, 0, sizeof(firmware));
    firmware.size = sizeof(firmware);
    result = _vshSblGetSystemSwVersion(&firmware);
    if (result < 0)
        return result;
    *version = firmware.version;
    return 0;
}

static int install_kernel_plugin(void) {
    int result = replace_payload(
        "install kernel plugin",
        APP_KERNEL_PLUGIN,
        KERNEL_PLUGIN_PATH
    );
    if (result < 0)
        return result;
    return update_tai_config(1);
}

static int remove_kernel_plugin(void) {
    int result = update_tai_config(0);
    if (result < 0)
        return result;
    return remove_payload("remove kernel plugin", KERNEL_PLUGIN_PATH);
}

static int install_browser_module(void) {
    int result = remove_payload(
        "remove legacy browser override",
        LEGACY_BROWSER_MODULE_PATH
    );
    if (result < 0)
        return result;

    return replace_payload(
        "install browser compatibility module",
        APP_BROWSER_MODULE,
        BROWSER_MODULE_PATH
    );
}

static int remove_browser_module(void) {
    int result = remove_payload(
        "remove browser compatibility module",
        BROWSER_MODULE_PATH
    );
    if (result < 0)
        return result;
    return remove_payload(
        "remove legacy browser override",
        LEGACY_BROWSER_MODULE_PATH
    );
}

static int install_app_tls_modules(void) {
    uint32_t version;
    int result;

    set_stage("query firmware version");
    result = get_firmware_version(&version);
    if (result < 0)
        return result;

    result = remove_payload(
        "remove legacy libhttp override",
        LEGACY_LIBHTTP_MODULE_PATH
    );
    if (result < 0)
        return result;

    result = replace_payload(
        "install libhttp module",
        APP_LIBHTTP_MODULE,
        LIBHTTP_MODULE_PATH
    );
    if (result < 0)
        return result;

    result = replace_payload(
        "install libssl module",
        APP_LIBSSL_MODULE,
        LIBSSL_MODULE_PATH
    );
    if (result < 0)
        return result;

    if (version == 0x03600000) {
        result = replace_payload(
            "install 3.60 DNS fix",
            APP_DNS_FIX_MODULE,
            DNS_FIX_MODULE_PATH
        );
        if (result < 0)
            return result;

        result = replace_payload(
            "install 3.60 commerce module",
            APP_COMMERCE_MODULE,
            COMMERCE_MODULE_PATH
        );
    }

    return result;
}

static int remove_app_tls_modules(void) {
    int result;

    result = remove_payload("remove libhttp module", LIBHTTP_MODULE_PATH);
    if (result < 0)
        return result;
    result = remove_payload("remove libssl module", LIBSSL_MODULE_PATH);
    if (result < 0)
        return result;
    result = remove_payload("remove 3.60 DNS fix", DNS_FIX_MODULE_PATH);
    if (result < 0)
        return result;
    result = remove_payload("remove 3.60 commerce module", COMMERCE_MODULE_PATH);
    if (result < 0)
        return result;
    return remove_payload(
        "remove legacy libhttp override",
        LEGACY_LIBHTTP_MODULE_PATH
    );
}

static int install_ca_bundle(void) {
    ItlsReplaceResult replace_result;

    set_stage("install CA certificate bundle");
    replace_result = itls_install_ca_bundle(
        &file_operations,
        APP_CA_BUNDLE,
        CA_ACTIVE_PATH,
        CA_BACKUP_PATH,
        CA_TEMP_PATH,
        CA_ROLLBACK_PATH
    );
    return apply_replace_result(replace_result);
}

static int restore_ca_bundle(void) {
    ItlsReplaceResult replace_result;

    set_stage("restore original CA certificate bundle");
    replace_result = itls_restore_ca_bundle(
        &file_operations,
        CA_ACTIVE_PATH,
        CA_BACKUP_PATH,
        CA_TEMP_PATH,
        CA_ROLLBACK_PATH
    );
    return apply_replace_result(replace_result);
}

static int install_full_package(void) {
    int result;

    result = install_browser_module();
    if (result < 0)
        return result;
    result = install_app_tls_modules();
    if (result < 0)
        return result;
    result = install_ca_bundle();
    if (result < 0)
        return result;
    return install_kernel_plugin();
}

static int remove_full_package(void) {
    int result;

    result = remove_kernel_plugin();
    if (result < 0)
        return result;
    result = remove_browser_module();
    if (result < 0)
        return result;
    result = remove_app_tls_modules();
    if (result < 0)
        return result;
    return restore_ca_bundle();
}

static int action_requires_vs0(MenuAction action) {
    return action != ACTION_INSTALL_PLUGIN
        && action != ACTION_REMOVE_PLUGIN
        && action != ACTION_EXIT;
}

static int execute_action(MenuAction action) {
    int result;

    last_warning = 0;
    last_stage = "prepare operation";

    if (action_requires_vs0(action)) {
        set_stage("mount vs0 read/write");
        result = mount_vs0_rw();
        if (result < 0)
            return result;
    }

    switch (action) {
        case ACTION_INSTALL_FULL:
            return install_full_package();
        case ACTION_INSTALL_PLUGIN:
            return install_kernel_plugin();
        case ACTION_INSTALL_BROWSER:
            return install_browser_module();
        case ACTION_INSTALL_APP_TLS:
            return install_app_tls_modules();
        case ACTION_INSTALL_CERTIFICATES:
            return install_ca_bundle();
        case ACTION_REMOVE_PLUGIN:
            return remove_kernel_plugin();
        case ACTION_REMOVE_BROWSER:
            return remove_browser_module();
        case ACTION_REMOVE_APP_TLS:
            return remove_app_tls_modules();
        case ACTION_RESTORE_CERTIFICATES:
            return restore_ca_bundle();
        case ACTION_REMOVE_FULL:
            return remove_full_package();
        case ACTION_EXIT:
        case ACTION_COUNT:
            return 0;
    }

    return ITLS_ERROR_INVALID_CONFIG;
}

static void render_menu(void) {
    int index;

    psvDebugScreenClear(COLOR_BLACK);
    psvDebugScreenSetFgColor(COLOR_CYAN);
    printf("                         iTLS-Enso 3.3                         \n");
    printf("                           By SKGleba                           \n\n");

    for (index = 0; index < ACTION_COUNT; index++) {
        psvDebugScreenSetFgColor(
            selected_action == index ? COLOR_GREEN : COLOR_WHITE
        );
        printf("%s\n", menu_items[index]);
    }

    psvDebugScreenSetFgColor(COLOR_GREY);
    printf("\nCross: select   Up/Down: navigate\n");
}

static uint32_t read_buttons(void) {
    SceCtrlData pad;
    memset(&pad, 0, sizeof(pad));
    sceCtrlPeekBufferPositive(0, &pad, 1);
    return pad.buttons;
}

static void wait_for_button_release(void) {
    while (read_buttons() != 0)
        sceKernelDelayThread(20 * 1000);
}

static void wait_for_circle(void) {
    for (;;) {
        uint32_t buttons = read_buttons();
        if (buttons & SCE_CTRL_CIRCLE) {
            wait_for_button_release();
            return;
        }
        sceKernelDelayThread(20 * 1000);
    }
}

static void show_action_result(int result) {
    psvDebugScreenClear(COLOR_BLACK);

    if (result < 0) {
        psvDebugScreenSetFgColor(COLOR_RED);
        printf("Operation failed\n\n");
        psvDebugScreenSetFgColor(COLOR_WHITE);
        printf("Stage: %s\n", last_stage);
        printf("Error: 0x%08X\n", (unsigned int)result);
        if (last_warning < 0)
            printf("Cleanup warning: 0x%08X\n", (unsigned int)last_warning);
        psvDebugScreenSetFgColor(COLOR_GREY);
        printf("\nPress Circle to return.\n");
        wait_for_circle();
        return;
    }

    psvDebugScreenSetFgColor(COLOR_GREEN);
    printf("Operation completed successfully.\n");
    if (last_warning < 0) {
        psvDebugScreenSetFgColor(COLOR_YELLOW);
        printf("Cleanup warning: 0x%08X\n", (unsigned int)last_warning);
    }
    psvDebugScreenSetFgColor(COLOR_WHITE);
    printf("\nPress Cross to reboot or Circle to return.\n");

    for (;;) {
        uint32_t buttons = read_buttons();
        if (buttons & SCE_CTRL_CROSS) {
            int reboot_result;
            wait_for_button_release();
            reboot_result = scePowerRequestColdReset();
            if (reboot_result >= 0) {
                sceKernelDelayThread(3 * 1000 * 1000);
                sceKernelExitProcess(0);
                return;
            }
            psvDebugScreenSetFgColor(COLOR_RED);
            printf("Reboot request failed: 0x%08X\n", (unsigned int)reboot_result);
            psvDebugScreenSetFgColor(COLOR_GREY);
            printf("Press Circle to return.\n");
            wait_for_circle();
            return;
        }
        if (buttons & SCE_CTRL_CIRCLE) {
            wait_for_button_release();
            return;
        }
        sceKernelDelayThread(20 * 1000);
    }
}

int main(void) {
    psvDebugScreenInit();
    wait_for_button_release();
    render_menu();

    for (;;) {
        uint32_t buttons = read_buttons();

        if (buttons & SCE_CTRL_CROSS) {
            MenuAction action = (MenuAction)selected_action;
            int result;

            wait_for_button_release();
            if (action == ACTION_EXIT)
                break;

            psvDebugScreenClear(COLOR_BLACK);
            psvDebugScreenSetFgColor(COLOR_GREEN);
            printf("Working...\n");
            result = execute_action(action);
            show_action_result(result);
            render_menu();
        } else if (buttons & SCE_CTRL_UP) {
            wait_for_button_release();
            if (selected_action > 0)
                selected_action--;
            render_menu();
        } else if (buttons & SCE_CTRL_DOWN) {
            wait_for_button_release();
            if (selected_action + 1 < ACTION_COUNT)
                selected_action++;
            render_menu();
        }

        sceKernelDelayThread(20 * 1000);
    }

    sceKernelExitProcess(0);
    return 0;
}
