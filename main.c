/*
	iTLS-Enso v3.0 by SKGleba
	All Rights Reserved
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <psp2/kernel/processmgr.h>
#include <psp2/ctrl.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include "graphics.h"

char mmit[][200] = {" -> Install/Uninstall iTLS"," -> Install/Uninstall the latest CA certs"," -> Exit"};

int optct = 3;

int sel = 0;
int i;
int shode = 0;

int ex(const char *fname) {
    FILE *file;
    if ((file = fopen(fname, "r")))
    {
        fclose(file);
        return 1;
    }
    return 0;
}

int riex(const char *fname) {
	int ret = ex(fname);
    if (ret == 1)
    	sceIoRemove(fname);
    return ret;
}

int fap(const char *from, const char *to) {
	long psz;
	FILE *fp = fopen(from,"rb");
	fseek(fp, 0, SEEK_END);
	psz = ftell(fp);
	rewind(fp);
	char* pbf = (char*) malloc(sizeof(char) * psz);
	fread(pbf, sizeof(char), (size_t)psz, fp);
	FILE *pFile = fopen(to, "ab");
	for (int i = 0; i < psz; ++i) {
			fputc(pbf[i], pFile);
	}
	fclose(fp);
	fclose(pFile);
	return 1;
}

int fcp(const char *from, const char *to) {
	SceUID fd = sceIoOpen(to, SCE_O_WRONLY | SCE_O_TRUNC | SCE_O_CREAT, 6);
	sceIoClose(fd);
	int ret = fap(from, to);
	return ret;
}

void smenu(){
	psvDebugScreenClear(COLOR_BLACK);
	psvDebugScreenSetFgColor(COLOR_CYAN);
	psvDebugScreenPrintf("                       iTLS-Enso 3.0                            \n");
	psvDebugScreenPrintf("                         By SKGleba                              \n");
	psvDebugScreenSetFgColor(COLOR_RED);
	for(i = 0; i < optct; i++){
		if(sel==i){
			psvDebugScreenSetFgColor(COLOR_GREEN);
		}
		psvDebugScreenPrintf("%s\n", mmit[i]);
		psvDebugScreenSetFgColor(COLOR_RED);
	}
	psvDebugScreenSetFgColor(COLOR_GREEN);
}

int work(int debug){
	if (sel == 2)
		sceKernelExitProcess(0);
	if (ex("ur0:tai/0syscall6.skprx") == 0 && debug == 0) {
		psvDebugScreenPrintf("WARNING: 0syscall6 is REQUIRED!\n");
		sceKernelDelayThread(5 * 1000 * 1000);
		sceKernelExitProcess(0);
	}
	void *buf = malloc(0x100);
	vshIoUmount(0x300, 0, 0, 0);
	vshIoUmount(0x300, 1, 0, 0);
	_vshIoMount(0x300, 0, 2, buf);
	psvDebugScreenPrintf("Working...\n");
	if (sel == 0) {
		if (ex("vs0:/data/external/webcore/ScePsp2Compat.supr0") == 0 && ex("vs0:/sys/external/libhttp.supr0") == 0) {
			psvDebugScreenPrintf("Installing iTLS... ");
			fcp("app0:compat.suprx", "vs0:/data/external/webcore/ScePsp2Compat.supr0");
			fcp("app0:lhttp.suprx", "vs0:/sys/external/libhttp.supr0");
			fcp("app0:lssl.suprx", "vs0:/sys/external/libssl.supr0");
			riex("vs0:/data/external/cert/CA_LIST.cer_old");
			sceIoRename("vs0:/data/external/cert/CA_LIST.cer", "vs0:/data/external/cert/CA_LIST.cer_old");
			fcp("app0:certs.cer", "vs0:/data/external/cert/CA_LIST.cer");
			fcp("app0:user.suprx", "ur0:tai/itls.suprx");
			fcp("app0:kernel.skprx", "ur0:tai/itls.skprx");
			if (ex("ux0:tai/config.txt") == 1 && ex("ux0:tai/config_preitls.txt") == 0)
				sceIoRename("ux0:tai/config.txt", "ux0:tai/config_preitls.txt");
			SceUID fd = sceIoOpen("ur0:tai/config_temp.txt", SCE_O_WRONLY | SCE_O_TRUNC | SCE_O_CREAT, 6);
			sceIoWrite(fd, (void *)"\n# ITLS\n*KERNEL\nur0:tai/itls.skprx\n*ALL\nur0:tai/itls.suprx\n", strlen("\n# ITLS\n*KERNEL\nur0:tai/itls.skprx\n*ALL\nur0:tai/itls.suprx\n"));
			sceIoClose(fd);
			fcp("ur0:tai/config.txt", "ur0:tai/config_preitls.txt");
			fap("ur0:tai/config_temp.txt", "ur0:tai/config.txt");
			sceIoRemove("ur0:tai/config_temp.txt");
		} else {
			psvDebugScreenPrintf("Uninstalling iTLS... ");
			if (ex("ur0:tai/config_preitls.txt") == 1) {
				sceIoRemove("ur0:tai/config.txt");
				sceIoRename("ur0:tai/config_preitls.txt", "ur0:tai/config.txt");
			}
			riex("ur0:tai/itls.suprx");
			riex("ur0:tai/itls.skprx");
			riex("vs0:/data/external/webcore/ScePsp2Compat.supr0");
			riex("vs0:/sys/external/libhttp.supr0");
			riex("vs0:/sys/external/libssl.supr0");
			if (ex("vs0:/data/external/cert/CA_LIST.cer_old") == 1) {
				riex("vs0:/data/external/cert/CA_LIST.cer");
				sceIoRename("vs0:/data/external/cert/CA_LIST.cer_old", "vs0:/data/external/cert/CA_LIST.cer");
				riex("vs0:/data/external/cert/CA_LIST.cer_old");
			}
		}
	} else if (sel == 1) {
		if (ex("vs0:/data/external/cert/CA_LIST.cer_old") == 1) {
			psvDebugScreenPrintf("Removing new CA certs... ");
			riex("vs0:/data/external/cert/CA_LIST.cer");
			sceIoRename("vs0:/data/external/cert/CA_LIST.cer_old", "vs0:/data/external/cert/CA_LIST.cer");
			riex("vs0:/data/external/cert/CA_LIST.cer_old");
		} else {
			psvDebugScreenPrintf("Adding new CA certs... ");
			sceIoRename("vs0:/data/external/cert/CA_LIST.cer", "vs0:/data/external/cert/CA_LIST.cer_old");
			fcp("app0:certs.cer", "vs0:/data/external/cert/CA_LIST.cer");
		}
	}
	psvDebugScreenPrintf("Done\n");
	sceKernelDelayThread(1 * 1000 * 1000);
	sceKernelExitProcess(0);
	return 0;
}

int main()
{
	psvDebugScreenInit();
	psvDebugScreenClear(COLOR_BLACK);
	psvDebugScreenSetFgColor(COLOR_GREEN);
	SceCtrlData pad;
	
	int l = 1;// happens
	
	smenu();
		
	while (l == 1) {
			sceCtrlPeekBufferPositive(0, &pad, 1);
			if (pad.buttons == SCE_CTRL_CROSS) {
				work(0);
				sel = 0;
				smenu();
				sceKernelDelayThread(0.35 * 1000 * 1000);
			} else if (pad.buttons == SCE_CTRL_UP) {
				if(sel!=0){
					sel--;
				}
				smenu();
				sceKernelDelayThread(0.3 * 1000 * 1000);
			} else if (pad.buttons == SCE_CTRL_DOWN) {
				if(sel+1<optct){
					sel++;
				}
				smenu();
				sceKernelDelayThread(0.3 * 1000 * 1000);
			} else if (pad.buttons == SCE_CTRL_SELECT) {
				work(1);
				sel = 0;
				smenu();
				sceKernelDelayThread(0.35 * 1000 * 1000);
			}
	}
	
	sceKernelDelayThread(10 * 1000 * 1000);
	sceKernelExitProcess(0);
    return 0;
}
