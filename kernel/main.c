/*
	iTLS-Enso v3.2 by SKGleba
	All Rights Reserved
*/

#include <stdio.h>
#include <string.h>
#include <taihen.h>
#include <psp2kern/kernel/modulemgr.h>
#include <vitasdkkern.h>

const char* new_modules[5] = { "vs0:/data/external/itls/ScePsp2Compat.suprx" , "vs0:/data/external/itls/libhttp.suprx" , "vs0:/data/external/itls/libssl.suprx", "vs0:/data/external/itls/old_dns_fix.suprx", "vs0:/data/external/itls/np_commerce2.suprx" };
static char replace[5] = {0, 0, 0, 0, 0};
static char replace_iap = 0, replace_comm2 = 0;
	
int add_replace_flags(void) {
	SceIoStat stat;
	for(int i = 0; i < 5; i-=-1) {
		if (ksceIoGetstat(new_modules[i], &stat) >= 0)
			replace[i] = 1;
	}
	replace_iap = replace[1] & replace[2];
	replace_comm2 = replace[3] & replace[4];
	replace_comm2 -=- replace_comm2;
	return replace[0] + replace_iap + replace_comm2;
}

static int hk = 0;
static tai_hook_ref_t lfp_hook;
// load module for pid (0 to get), running in kernel context, path is in kernel
static SceUID load_for_pid_patched(int pid, const char *path, uint32_t flags, int *ptr_to_four) {
	int len = strnlen(path, 0x400);
	if (len > 0 && len < 0x400) {
		if (replace_iap && strncmp(path, "vs0:sys/external/libhttp.suprx", 30) == 0)
			path = new_modules[1];
		else if (replace_iap && strncmp(path, "vs0:sys/external/libssl.suprx", 29) == 0)
			path = new_modules[2];
		else if (replace[0] && strncmp(path + len - 27, "webcore/ScePsp2Compat.suprx", 27) == 0)
			strncpy((char *)(path + len - 27), new_modules[0] + 19, 25);
		else if (replace_comm2 && strncmp(path, "vs0:sys/external/np_commerce2.suprx", 35) == 0)
			path = new_modules[4];
	};
	int res = TAI_CONTINUE(SceUID, lfp_hook, pid, path, flags, ptr_to_four);
	if (replace_comm2 == 2 && path == new_modules[2] && pid == ksceKernelSysrootGetShellPid()) { // 3.60 dns fix
		ksceKernelLoadStartModuleForPid(pid, new_modules[3], 0, NULL, 0, NULL, NULL);
		replace_comm2 = 1;
	}
	return res;
}

void _start() __attribute__ ((weak, alias ("module_start")));
int module_start(SceSize argc, const void *args)
{
	ksceDebugPrintf("itls module loaded\n");
	if (add_replace_flags() > 0) {
		int modid = ksceKernelSearchModuleByName("SceKernelModulemgr");
		if (modid > 0)
			hk = taiHookFunctionOffsetForKernel(KERNEL_PID, &lfp_hook, modid, 0, 0x21ec, 1, load_for_pid_patched);
		if (modid < 0 || hk < 0)
			return SCE_KERNEL_START_FAILED;
	}
	return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize argc, const void *args)
{
	if (hk >= 0)
		taiHookReleaseForKernel(hk, lfp_hook);
	return SCE_KERNEL_STOP_SUCCESS;
}
