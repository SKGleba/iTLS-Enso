/*
	iTLS-Enso v3.0 by SKGleba
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

void _start() __attribute__ ((weak, alias ("module_start")));
int module_start(SceSize argc, const void *args)
{
	char zero[1] = {0x30};
	INJECT("SceSysmodule", 0x162c, zero, sizeof(zero));
	INJECT("SceSysmodule", 0x163b, zero, sizeof(zero));
	return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize argc, const void *args)
{
	return SCE_KERNEL_STOP_SUCCESS;
}
