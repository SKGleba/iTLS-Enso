/*
  iTLS-Enso v3.2 by SKGleba
  All Rights Reserved
*/

#include <psp2/io/dirent.h>
#include <psp2/io/fcntl.h>
#include <psp2/kernel/clib.h>
#include <psp2/kernel/modulemgr.h>

#include <taihen.h>

static SceUID httpConnHookID;
static tai_hook_ref_t httpConnHookRef;

static int httpConnPatched(int tmplId, const char* serverName, const char* scheme, unsigned short port, int enableKeepalive) {
  if (sceClibStrncmp("native.np.ac.playstation.net", serverName, 29) == 0)
    serverName = "native-vita.np.ac.playstation.net";
  return TAI_CONTINUE(int, httpConnHookRef, tmplId, serverName, scheme, port, enableKeepalive);
}

void _start() __attribute__((weak, alias("module_start")));
int module_start(SceSize args, void* argp) {
  tai_module_info_t info;
  info.size = sizeof(info);
  if (taiGetModuleInfo("SceShell", &info) >= 0)
    httpConnHookID = taiHookFunctionImport(&httpConnHookRef, "SceShell", 0xE8F15CDE, 0xAEB3307E, httpConnPatched);
  return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize args, void* argp) {
  if (httpConnHookID >= 0)
    taiHookRelease(httpConnHookID, httpConnHookRef);
  return SCE_KERNEL_STOP_SUCCESS;
}
