/*
	iTLS-Enso v3.3 by SKGleba
	All Rights Reserved
*/

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <taihen.h>
#include <psp2kern/io/fcntl.h>
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
static const char diagnostic_log_path[] = "ur0:tai/itls.log";
static SceUID diagnostic_log_fd = -1;
static unsigned int module_load_sequence = 0;

static void diagnostic_log_open(void) {
	diagnostic_log_fd = ksceIoOpen(
		diagnostic_log_path,
		SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC,
		0666
	);
	ksceDebugPrintf(
		"itls log open: path=%s result=0x%08X\n",
		diagnostic_log_path,
		(unsigned int)diagnostic_log_fd
	);
}

static void diagnostic_log_close(void) {
	if (diagnostic_log_fd >= 0) {
		ksceIoClose(diagnostic_log_fd);
		diagnostic_log_fd = -1;
	}
}

static void diagnostic_logf(const char *format, ...)
    __attribute__((format(printf, 1, 2)));

static void diagnostic_logf(const char *format, ...) {
	char buffer[512];
	va_list arguments;
	int length;

	va_start(arguments, format);
	length = vsnprintf(buffer, sizeof(buffer), format, arguments);
	va_end(arguments);

	if (length < 0)
		return;
	if ((size_t)length >= sizeof(buffer))
		length = sizeof(buffer) - 1;

	ksceDebugPrintf("%s", buffer);
	if (diagnostic_log_fd >= 0) {
		int written = ksceIoWrite(diagnostic_log_fd, buffer, length);
		if (written != length) {
			ksceDebugPrintf(
				"itls log write incomplete: requested=%d result=0x%08X\n",
				length,
				(unsigned int)written
			);
		}
	}
}

static int add_replace_flags(void) {
	int available[ITLS_REPLACEMENT_MODULE_COUNT] = {0};
	SceKernelFwInfo firmware;
	SceIoStat stat;
	int firmware_result;
	int i;

	for (i = 0; i < ITLS_REPLACEMENT_MODULE_COUNT; i++) {
		int stat_result;

		memset(&stat, 0, sizeof(stat));
		stat_result = ksceIoGetstat(new_modules[i], &stat);
		if (stat_result >= 0)
			available[i] = 1;
		diagnostic_logf(
			"itls replacement[%d] stat=0x%08X path=%s\n",
			i,
			(unsigned int)stat_result,
			new_modules[i]
		);
	}

	memset(&firmware, 0, sizeof(firmware));
	firmware.size = sizeof(firmware);
	firmware_result = ksceKernelGetSystemSwVersion(&firmware);
	if (firmware_result < 0) {
		diagnostic_logf(
			"itls firmware query failed: 0x%08X\n",
			(unsigned int)firmware_result
		);
		firmware.version = 0;
	}

	replacement_state = itls_build_replacement_state(
		available,
		firmware.version
	);
	dns_fix_pending = replacement_state.commerce;

	diagnostic_logf(
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
	char rewrite_buffer[ITLS_PATH_REWRITE_BUFFER_SIZE];
	const char *selected_path = path;
	const char *path_for_log;
	const char *selected_path_for_log;
	unsigned int sequence = __atomic_add_fetch(
		&module_load_sequence,
		1,
		__ATOMIC_RELAXED
	);
	int load_dns_fix = 0;
	int path_valid = 0;
	int trace_load = sequence == 1;
	SceUID result;

	if (path != NULL) {
		size_t length = strnlen(path, 0x400);
		if (length > 0 && length < 0x400) {
			path_valid = 1;
			selected_path = itls_select_replacement_path(
				&replacement_state,
				path,
				rewrite_buffer,
				sizeof(rewrite_buffer)
			);
			if (itls_is_diagnostic_module_path(path))
				trace_load = 1;
		}
	}

	if (selected_path != path)
		trace_load = 1;

	path_for_log = path_valid ? path : (path == NULL ? "<null>" : "<invalid>");
	selected_path_for_log = selected_path == path
		? path_for_log
		: (selected_path == NULL ? "<null>" : selected_path);

	if (trace_load) {
		diagnostic_logf(
			"itls load[%u] request: pid=0x%08X flags=0x%08X path=%s selected=%s\n",
			sequence,
			(unsigned int)pid,
			(unsigned int)flags,
			path_for_log,
			selected_path_for_log
		);
	}

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

	if (trace_load) {
		diagnostic_logf(
			"itls load[%u] result: result=0x%08X pid=0x%08X selected=%s\n",
			sequence,
			(unsigned int)result,
			(unsigned int)pid,
			selected_path_for_log
		);
	}

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
		diagnostic_logf(
			"itls dns fix load: pid=0x%08X result=0x%08X\n",
			(unsigned int)pid,
			(unsigned int)dns_result
		);
		if (dns_result >= 0)
			dns_fix_pending = 0;
	}

	return result;
}

void _start() __attribute__ ((weak, alias ("module_start")));
int module_start(SceSize argc, const void *args)
{
	int replacement_count;
	int modid = -1;

	(void)argc;
	(void)args;

	diagnostic_log_open();
	diagnostic_logf(
		"itls module start: build=3.3-diagnostic-3 log_fd=0x%08X shell_pid=0x%08X\n",
		(unsigned int)diagnostic_log_fd,
		(unsigned int)ksceKernelSysrootGetShellPid()
	);

	replacement_count = add_replace_flags();
	if (replacement_count > 0) {
		modid = ksceKernelSearchModuleByName("SceKernelModulemgr");
		if (modid > 0) {
			hk = taiHookFunctionOffsetForKernel(
				KERNEL_PID,
				&lfp_hook,
				modid,
				0,
				0x21ec,
				1,
				load_for_pid_patched
			);
		}
		diagnostic_logf(
			"itls hook install: replacements=%d module=0x%08X offset=0x000021EC result=0x%08X\n",
			replacement_count,
			(unsigned int)modid,
			(unsigned int)hk
		);
		if (modid <= 0 || hk < 0) {
			diagnostic_logf("itls module start failed\n");
			diagnostic_log_close();
			return SCE_KERNEL_START_FAILED;
		}
	} else {
		diagnostic_logf("itls hook skipped: no complete replacement set\n");
	}

	diagnostic_logf("itls module start succeeded\n");
	return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize argc, const void *args)
{
	(void)argc;
	(void)args;

	if (hk >= 0) {
		int release_result = taiHookReleaseForKernel(hk, lfp_hook);
		diagnostic_logf(
			"itls hook release: hook=0x%08X result=0x%08X\n",
			(unsigned int)hk,
			(unsigned int)release_result
		);
	}
	diagnostic_logf("itls module stopped\n");
	diagnostic_log_close();
	return SCE_KERNEL_STOP_SUCCESS;
}
