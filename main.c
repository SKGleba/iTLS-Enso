/*
	iTLS-Enso v1.5 by SKGleba
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

char mmit[][150] = {" -> Install 3.68 compat module"," -> Uninstall 3.68 compat module"," -> Install 3.68 root certs"," -> Exit"};
int optct = 4;

int sel = 0;
int i;
int shode = 0;

int fcp(const char *from, const char *to) {
	long fsz;
	FILE *fp = fopen(from,"rb");

	fseek(fp, 0, SEEK_END);
	fsz = ftell(fp);
	rewind(fp);

	char* fbuf = (char*) malloc(sizeof(char) * fsz);
	fread(fbuf, sizeof(char), (size_t)fsz, fp);

	FILE *pFile = fopen(to, "wb");
	
	for (int i = 0; i < fsz; ++i) {
			fputc(fbuf[i], pFile);
	}
   
	fclose(fp);
	fclose(pFile);
	return 1;
}

int ex(const char *fname) {
    FILE *file;
    if ((file = fopen(fname, "r")))
    {
        fclose(file);
        return 1;
    }
    return 0;
}

int frcp(const char *from, const char *to) {
if (ex(to) == 1) sceIoRemove(to);
	long fsz;
	FILE *fp = fopen(from,"rb");

	fseek(fp, 0, SEEK_END);
	fsz = ftell(fp);
	rewind(fp);

	char* fbuf = (char*) malloc(sizeof(char) * fsz);
	fread(fbuf, sizeof(char), (size_t)fsz, fp);

	FILE *pFile = fopen(to, "wb");
	
	for (int i = 0; i < fsz; ++i) {
			fputc(fbuf[i], pFile);
	}
   
	fclose(fp);
	fclose(pFile);
	return 1;
}


void smenu(){
	psvDebugScreenClear(COLOR_BLACK);
	psvDebugScreenSetFgColor(COLOR_CYAN);
	psvDebugScreenPrintf("                       iTLS-Enso v1.5                            \n");
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

int do_shit(){
if (sel == 3) sceKernelExitProcess(0);
void *buf = malloc(0x100);
vshIoUmount(0x300, 0, 0, 0);
vshIoUmount(0x300, 1, 0, 0);
_vshIoMount(0x300, 0, 2, buf);
psvDebugScreenPrintf("Working...\n");
if (ex("vs0:/data/external/webcore/cpt.o") == 0) fcp("vs0:/data/external/webcore/ScePsp2Compat.suprx", "vs0:/data/external/webcore/cpt.o");
if (sel == 0) frcp("app0:Media/00", "vs0:/data/external/webcore/ScePsp2Compat.suprx");
if (sel == 1) frcp("vs0:/data/external/webcore/cpt.o", "vs0:/data/external/webcore/ScePsp2Compat.suprx");
if (sel == 2) frcp("app0:Media/01", "vs0:/data/external/cert/CA_LIST.cer");
psvDebugScreenPrintf("Done\n");
sceKernelDelayThread(1 * 1000 * 1000);sceKernelExitProcess(0);}
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
				do_shit();
				sel = 0;
				smenu();
				sceKernelDelayThread(0.35 * 1000 * 1000);
			}
			
			if (pad.buttons == SCE_CTRL_UP) {
				if(sel!=0){
					sel--;
				}
				smenu();
				sceKernelDelayThread(0.3 * 1000 * 1000);
			}
			
			if (pad.buttons == SCE_CTRL_DOWN) {
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
