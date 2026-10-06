/*
	iTLS-Enso v3.3 by SKGleba
	All Rights Reserved
*/

#include <stdio.h>
#include <string.h>
#include <taihen.h>
#include <psp2kern/kernel/modulemgr.h>
#include <vitasdkkern.h>

#include "path_rewrite.h"

static const char *const new_modules[ITLS_REPLACEMENT_MODULE_COUNT] = {
	"vs0:/data/external/itls/ScePsp2Compat.suprx",
	"vs0:/data/external/itls/libhttp.suprx",
	"vs0:/data/external/itls/libssl.suprx",
	"vs0:/data/external/itls/old_dns_fix.suprx",
	"vs0:/data/external/itls/np_commerce2.suprx"
};
static ItlsReplacementState replacement_state;
static int dns_fix_pending = 0;

static int add_replace_flags(void) {
	int available[ITLS_REPLACEMENT_MODULE_COUNT] = {0};
	SceKernelFwInfo firmware;
	SceIoStat stat;
	int firmware_result;
	int i;

	for (i = 0; i < ITLS_REPLACEMENT_MODULE_COUNT; i++) {
		memset(&stat, 0, sizeof(stat));
		if (ksceIoGetstat(new_modules[i], &stat) >= 0)
			available[i] = 1;
	}

	memset(&firmware, 0, sizeof(firmware));
	firmware.size = sizeof(firmware);
	firmware_result = ksceKernelGetSystemSwVersion(&firmware);
	if (firmware_result < 0) {
		ksceDebugPrintf(
			"itls firmware query failed: 0x%08X\n",
			firmware_result
		);
		firmware.version = 0;
	}

	replacement_state = itls_build_replacement_state(
		available,
		firmware.version
	);
	dns_fix_pending = replacement_state.commerce;

	ksceDebugPrintf(
		"itls firmware=0x%08X replacements: browser=%d app_tls=%d commerce=%d\n",
		firmware.version,
		replacement_state.browser,
		replacement_state.app_tls,
		replacement_state.commerce
	);

	return replacement_state.browser
		+ replacement_state.app_tls
		+ replacement_state.commerce;
}

static int hk = -1;
static tai_hook_ref_t lfp_hook;
static SceUID load_for_pid_patched(int pid, const char *path, uint32_t flags, int *ptr_to_four) {
	const char *selected_path = path;
	int load_dns_fix = 0;
	SceUID result;

	if (path != NULL) {
		size_t length = strnlen(path, 0x400);
		if (length > 0 && length < 0x400)
			selected_path = itls_select_replacement_path(&replacement_state, path);
	}

	if (selected_path != path)
		ksceDebugPrintf("itls redirect: %s -> %s\n", path, selected_path);

	if (
		dns_fix_pending
		&& selected_path != NULL
		&& strcmp(selected_path, new_modules[2]) == 0
		&& pid == ksceKernelSysrootGetShellPid()
	)
		load_dns_fix = 1;

	result = TAI_CONTINUE(
		SceUID,
		lfp_hook,
		pid,
		selected_path,
		flags,
		ptr_to_four
	);

	if (load_dns_fix && result >= 0) {
		SceUID dns_result = ksceKernelLoadStartModuleForPid(
			pid,
			new_modules[3],
			0,
			NULL,
			0,
			NULL,
			NULL
		);
		ksceDebugPrintf("itls dns fix load: 0x%08X\n", dns_result);
		if (dns_result >= 0)
			dns_fix_pending = 0;
	}

	return result;
}

void _start() __attribute__ ((weak, alias ("module_start")));
int module_start(SceSize argc, const void *args)
{
	(void)argc;
	(void)args;

	ksceDebugPrintf("itls module loaded\n");
	if (add_replace_flags() > 0) {
		int modid = ksceKernelSearchModuleByName("SceKernelModulemgr");
		if (modid > 0)
			hk = taiHookFunctionOffsetForKernel(KERNEL_PID, &lfp_hook, modid, 0, 0x21ec, 1, load_for_pid_patched);
		if (modid <= 0 || hk < 0)
			return SCE_KERNEL_START_FAILED;
	}
	return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize argc, const void *args)
{
	(void)argc;
	(void)args;

	if (hk >= 0)
		taiHookReleaseForKernel(hk, lfp_hook);
	return SCE_KERNEL_STOP_SUCCESS;
}
