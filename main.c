/*
	iTLS-Enso v3.1 by SKGleba
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

#define printf psvDebugScreenPrintf

char mmit[][512] = {" -> Install the full iTLS package"," -> Install the iTLS plugin"," -> Install the iTLS compat module (web-browser)"," -> Install the iTLS http & ssl modules (in-app)"," -> Install the iTLS CA certs"," -> Exit"};
char mmut[][512] = {" -> Uninstall the full iTLS package"," -> Uninstall the iTLS plugin"," -> Uninstall the iTLS compat module (web-browser)"," -> Uninstall the iTLS http & ssl modules (in-app)"," -> Uninstall the iTLS CA certs"," -> Exit"};

int optct = 6;

int sel = 0;
int i;
int shode = 0;
char cfg[5] = {0, 0, 0, 0, 0};

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
	printf("                        iTLS-Enso 3.1                            \n");
	printf("                         By SKGleba                              \n");
	psvDebugScreenSetFgColor(COLOR_RED);
	for(i = 0; i < optct; i++){
		if(sel==i){
			psvDebugScreenSetFgColor(COLOR_GREEN);
		}
		printf("%s\n", (cfg[i] == 0) ? mmit[i] : mmut[i]);
		psvDebugScreenSetFgColor(COLOR_RED);
	}
	psvDebugScreenSetFgColor(COLOR_GREEN);
}

void installPlugin(int ior) {
	if (cfg[1] == ior || ior == 69) {
		if (cfg[1] == 1) {
			printf("Removing the iTLS plugin... \n");
			riex("ur0:tai/itls.skprx");
			if (ex("ur0:tai/config_preitls.txt") == 1) {
				sceIoRemove("ur0:tai/config.txt");
				sceIoRename("ur0:tai/config_preitls.txt", "ur0:tai/config.txt");
			}
		} else {
			printf("Installing the iTLS plugin... \n");
			fcp("app0:kernel.skprx", "ur0:tai/itls.skprx");
			if (ex("ux0:tai/config.txt") == 1 && ex("ux0:tai/config_preitls.txt") == 0)
				sceIoRename("ux0:tai/config.txt", "ux0:tai/config_preitls.txt");
			SceUID fd = sceIoOpen("ur0:tai/config_temp.txt", SCE_O_WRONLY | SCE_O_TRUNC | SCE_O_CREAT, 6);
			sceIoWrite(fd, (void *)"\n# ITLS\n*KERNEL\nur0:tai/itls.skprx\n", strlen("\n# ITLS\n*KERNEL\nur0:tai/itls.skprx\n"));
			sceIoClose(fd);
			fcp("ur0:tai/config.txt", "ur0:tai/config_preitls.txt");
			fap("ur0:tai/config_temp.txt", "ur0:tai/config.txt");
			sceIoRemove("ur0:tai/config_temp.txt");
		}
	}
}

void installCompat(int ior) {
	if (cfg[2] == ior || ior == 69) {
		if (cfg[2] == 1) {
			printf("Removing the iTLS compat module... \n");
			riex("vs0:/data/external/webcore/ScePsp2Compat.supr0");
		} else {
			printf("Installing the iTLS compat module... \n");
			fcp("app0:compat.suprx", "vs0:/data/external/webcore/ScePsp2Compat.supr0");
		}
	}
}

void installIapp(int ior) {
	if (cfg[3] == ior || ior == 69) {
		if (cfg[3] == 1) {
			printf("Removing the iTLS http & ssl modules... \n");
			riex("vs0:/sys/external/libhttp.supr0");
			riex("vs0:/sys/external/libssl.supr0");
		} else {
			printf("Installing the iTLS http & ssl modules... \n");
			fcp("app0:lhttp.suprx", "vs0:/sys/external/libhttp.supr0");
			fcp("app0:lssl.suprx", "vs0:/sys/external/libssl.supr0");
		}
	}
}
	
void installCerts(int ior) {
	if (cfg[4] == ior || ior == 69) {
		if (cfg[4] == 1) {
			printf("Removing iTLS CA certs... \n");
			riex("vs0:/data/external/cert/CA_LIST.cer");
			sceIoRename("vs0:/data/external/cert/CA_LIST.cer_old", "vs0:/data/external/cert/CA_LIST.cer");
			riex("vs0:/data/external/cert/CA_LIST.cer_old");
		} else {
			printf("Adding iTLS CA certs... \n");
			sceIoRename("vs0:/data/external/cert/CA_LIST.cer", "vs0:/data/external/cert/CA_LIST.cer_old");
			fcp("app0:certs.cer", "vs0:/data/external/cert/CA_LIST.cer");
		}
	}
}

int work(){
	if (sel == 5)
		sceKernelExitProcess(0);
	void *buf = malloc(0x100);
	vshIoUmount(0x300, 0, 0, 0);
	vshIoUmount(0x300, 1, 0, 0);
	_vshIoMount(0x300, 0, 2, buf);
	printf("Working...\n");
	if (sel == 0) {
		if (cfg[0] == 0) {
			printf("Installing iTLS... \n");
			installPlugin(0);
			installCompat(0);
			installIapp(0);
			installCerts(0);
		} else {
			printf("Uninstalling iTLS... \n");
			installPlugin(1);
			installCompat(1);
			installIapp(1);
			installCerts(1);
		}
	} else if (sel == 1) {
		installPlugin(69);
	} else if (sel == 2) {
		installCompat(69);
	} else if (sel == 3) {
		installIapp(69);
	} else if (sel == 4) {
		installCerts(69);
	}
	printf("Done, please reboot the device\n");
	sceKernelDelayThread(5 * 1000 * 1000);
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
	
	if (ex("ur0:tai/itls.suprx") == 1)
		sceIoRemove("ur0:tai/itls.suprx");
	if (ex("ur0:tai/itls.skprx") == 1)
		cfg[1] = 1;
	if (ex("vs0:/data/external/webcore/ScePsp2Compat.supr0") == 1)
		cfg[2] = 1;
	if (ex("vs0:/sys/external/libhttp.supr0") == 1)
		cfg[3] = 1;
	if (ex("vs0:/data/external/cert/CA_LIST.cer_old") == 1)
		cfg[4] = 1;
	
	if (cfg[1] == 1 && cfg[2] == 1 && cfg[3] == 1 && cfg[4] == 1)
		cfg[0] = 1;
	
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
			}
	}
	
	sceKernelDelayThread(10 * 1000 * 1000);
	sceKernelExitProcess(0);
    return 0;
}
