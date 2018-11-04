/*
	iTLS-Enso v2.0 by SKGleba
	All Rights Reserved
*/
// Requires ioplus v0.1 or higher
#include <psp2/kernel/modulemgr.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <taihen.h>

static int hk;
static tai_hook_ref_t lum_hook;
static SceUID lum_patch(char *path, SceSize args, void *argp, int flags, SceKernelLMOption *option, int *status) {
	int pathlen = strlen(path);
	if (path[pathlen - 14] == 0x70 && path[pathlen - 13] == 0x32 && path[pathlen - 12] == 0x43) path = "ur0:itls/compat.suprx";
	SceUID ret = TAI_CONTINUE(SceUID, lum_hook, path, args, argp, flags, option, status);
	return ret;
}

void _start() __attribute__ ((weak, alias("module_start")));
int module_start(SceSize args, void *argp) {
	hk = taiHookFunctionImport(&lum_hook, TAI_MAIN_MODULE, TAI_ANY_LIBRARY, 0x2DCC4AFA, lum_patch);
	return 0;
}

int module_stop(SceSize args, void *argp) {
	if(hk >= 0)taiHookRelease(hk, lum_hook);
	return SCE_KERNEL_STOP_SUCCESS;
}
