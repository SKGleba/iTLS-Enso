/*
	iTLS-Enso v3.1 by SKGleba
	All Rights Reserved
*/

#include <stdio.h>
#include <string.h>
#include <taihen.h>
#include <psp2kern/kernel/modulemgr.h>
#include <vitasdkkern.h>

#define INJECT(name, off, data, sz)   \
do {                                \
	uintptr_t addr;					\
	tai_module_info_t info;			\
	info.size = sizeof(info);		\
	if (module_get_by_name_nid(KERNEL_PID, name, TAI_IGNORE_MODULE_NID, &info) >= 0) {	\
		module_get_offset(KERNEL_PID, info.modid, 0, off, &addr); \
		ksceKernelCpuUnrestrictedMemcpy((void *)addr, (void *)data, sz);	\
	}									\
} while (0)
	
int ex(char *path) {
	SceIoStat stat;
	int stat_ret = ksceIoGetstat(path, &stat);
	if (stat_ret < 0)
		return 0;
	return 1;
}
	
uint8_t pmode = 0;
static char path_k[0x80];
static int hk;
static tai_hook_ref_t lum_hook;
static SceUID lum_patch(char *path, SceSize args, void *argp, int flags, SceKernelLMOption *option, int *status) {
	int state;
	ENTER_SYSCALL(state);
	int pathlen = ksceKernelStrnlenUser((uintptr_t)path, 0x80);
	if (pathlen < 0x80) {
		ksceKernelMemcpyUserToKernel(path_k, (uintptr_t)path, pathlen);
		if (path_k[pathlen - 14] == 0x70 && path_k[pathlen - 13] == 0x32 && path_k[pathlen - 12] == 0x43) {
			path_k[pathlen - 1] = 0x30;
			ksceKernelMemcpyKernelToUser((uintptr_t)path, path_k, pathlen);
		}
	}
	EXIT_SYSCALL(state);
	SceUID ret = TAI_CONTINUE(SceUID, lum_hook, path, args, argp, flags, option, status);
	return ret;
}

void _start() __attribute__ ((weak, alias ("module_start")));
int module_start(SceSize argc, const void *args)
{
	char zero[1] = {0x30};
	if (ex("vs0:/data/external/webcore/ScePsp2Compat.supr0") == 1) {
		INJECT("SceSysmodule", 0x162c, zero, sizeof(zero));
		INJECT("SceSysmodule", 0x163b, zero, sizeof(zero));
		pmode = 1;
	}
	if (ex("vs0:/sys/external/libhttp.supr0") == 1) {
		hk = taiHookFunctionExportForKernel(KERNEL_PID, &lum_hook, "SceKernelModulemgr", TAI_ANY_LIBRARY, 0x60647592, lum_patch);
		pmode = pmode + 0x10;
	}
	return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize argc, const void *args)
{
	if (pmode >= 0x10) {
		if (hk >= 0)
			taiHookReleaseForKernel(hk, lum_hook);
		pmode = pmode - 0x10;
	}
	char x[1] = {0x7A};
	if (pmode == 1) {
		INJECT("SceSysmodule", 0x162c, x, sizeof(x));
		INJECT("SceSysmodule", 0x163b, x, sizeof(x));
	}
	return SCE_KERNEL_STOP_SUCCESS;
}
