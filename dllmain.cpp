#include <stdio.h>
#include "MemoryMgr.h"

#define REVERSE(a) ((a & 0xFF) << 24 | (a & 0xFF00) << 8 | (a & 0xFF0000) >> 8 | (a & 0xFF000000) >> 24)

typedef uint64_t uint64;
typedef uint32_t uint32;
typedef uint16_t uint16;
typedef uint8_t uint8;

struct effects {
	uint8 enable_heathaze     : 1;
	uint8 enable_grass        : 1;
	uint8 enable_speedblur    : 1;
	uint8 enable_sunflare     : 1;
	uint8 enable_pedshadows   : 1;
	uint8 enable_zonenames    : 1;
	uint8 enable_vehiclenames : 1;
} effects;

bool patched = false;
char reloadkey = VK_F9;
char gamedir[261];
uint64 addrbackup;
uint64 *createfile_orig;

/* GTA SA natives */
WRAPPER void CTimeCycle__Update(void) { EAXJMP(0x561760); }

void
get_values(void)
{
	char line[64];
	char *c;
	uint8 val;
	FILE *fd = fopen("sampfix.ini", "rb");

	if (fd) {
		while (fgets(line, sizeof(line), fd)) {
			if (line[0] == ';' || line[0] == '\n')
				continue;
			for (c = line; *c != '='; c++);
			if (atoi(c + 1) > 0)
				val = 1;
			else
				val = 0;
			*c = '\0';

			if (strcmp(line, "ReloadKey") == 0)
				sscanf(c + 1, "%X", &reloadkey);
			else if (strcmp(line, "EnableGrass") == 0)
				effects.enable_grass = val;
			else if (strcmp(line, "EnableHeatHaze") == 0)
				effects.enable_heathaze = val;
			else if (strcmp(line, "EnableSpeedBlur") == 0)
				effects.enable_speedblur = val;
			else if (strcmp(line, "EnableSunFlare") == 0)
				effects.enable_sunflare = val;
			else if (strcmp(line, "EnablePedShadows") == 0)
				effects.enable_pedshadows = val;
			else if (strcmp(line, "EnableZoneNames") == 0)
				effects.enable_zonenames = val;
			else if (strcmp(line, "EnableVehicleNames") == 0)
				effects.enable_vehiclenames = val;
		}
		fclose(fd);
	}
}

const char *
filename_from_path(const char *path)
{
	const char *p = path;
	int len = strlen(path);

	if (p != NULL) {
		if (len > 0) {
			p = path + len - 1;
			while (p != path && *p != '\\' && *p != '/')
				p--;
			if (*p == '\\' || *p == '/')
				p++;
		}
	}
	return p;
}

void
fix_samp(void)
{
	/* grass */
	Patch<uint32, uint32>(0x53C159, REVERSE(0xE8420E0A));
	Patch<uint8>(0x53C159 + 4, 0x00);
	/* sunflare */
	Patch<uint32, uint32>(0x53C136, REVERSE(0xE865041C));
	Patch<uint8>(0x53C136 + 4, 0x00);
	/*  ped shadows */
	Patch<uint32, uint32>(0x53EA08, REVERSE(0xB95003C4));
	Patch<uint8>(0x53EA08 + 4, 0x00);
	Patch<uint32, uint32>(0x53EA0D, REVERSE(0xE89E801C));
	Patch<uint8>(0x53EA0D + 4, 0x00);
	/* zone names */
	Patch<uint32, uint32>(0x5720A5, REVERSE(0xE876FEFF));
	Patch<uint8>(0x5720A5 + 4, 0xFF);
	/* fade out logos */
	Patch<uint32, uint32>(0x590099, REVERSE(0xE8C27A19));
	Patch<uint8>(0x590099 + 4, 0x00);
	/* unknown */
	Patch<uint32, uint32>(0x5952A6, REVERSE(0xE885CFFF));
	Patch<uint8>(0x5952A6 + 4, 0xFF);
	/* vehicle names */
	Patch<uint32, uint32>(0x58FBE9, REVERSE(0xE8B2B2FF));
	Patch<uint8>(0x58FBE9 + 4, 0xFF);
	
	patched = true;
}

void
apply_patches(void)
{
	if (effects.enable_heathaze)
		Patch<uint8>(0x701780, 0x83);
	else
		Patch<uint8>(0x701780, 0xC3); /* return earlier */

	if (effects.enable_speedblur) {
		Patch<uint32, uint32>(0x704E8A, REVERSE(0xE811E2FF));
		Patch<uint8>(0x704E8A + 4, 0xFF);
	} else {
		Nop<uint32>(0x704E8A, 5);
	}

	if (effects.enable_grass)
		Patch<uint8>(0x5DBAED, 0x85);
	else
		Patch<uint8>(0x5DBAED, 0x33);

	if (effects.enable_sunflare) {
		Patch<uint32, uint32>(0x53C136, REVERSE(0xE865041C));
		Patch<uint8>(0x53C136 + 4, 0x00);
	} else {
		Nop<uint32>(0x53C136, 5);
	}

	if (effects.enable_pedshadows) {
		Patch<uint32, uint32>(0x53EA0D, REVERSE(0xE89E801C));
		Patch<uint8>(0x53EA0D + 4, 0x00);
	} else {
		Nop<uint32>(0x53EA0D, 5);
	}

	if (effects.enable_zonenames) {
		Patch<uint32, uint32>(0x5720A5, REVERSE(0xE876FEFF));
		Patch<uint8>(0x5720A5 + 4, 0xFF);
	} else {
		Nop<uint32>(0x5720A5, 5);
	}

	if (effects.enable_vehiclenames) {
		Patch<uint32, uint32>(0x58FBE9, REVERSE(0xE8B2B2FF));
		Patch<uint8>(0x58FBE9 + 4, 0xFF);
	} else {
		Nop<uint32>(0x58FBE9, 5);
	}
}

int __stdcall
createfile_hooked(_In_ const char *lpFileName,
	_In_ uint32 dwDesiredAccess, _In_ uint32 dwShareMode,
	_In_opt_ _SECURITY_ATTRIBUTES *lpSecurityAttributes,
	_In_ uint32 dwCreationDisposition, _In_ uint32 dwFlagsAndAttributes,
	_In_opt_ void *hTemplateFile)
{
	if (lpFileName == NULL)
		return -1;

	const char *filename = filename_from_path(lpFileName);
	*createfile_orig = addrbackup;
	int fd = (int)CreateFileA(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes,
		dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
	addrbackup = *createfile_orig;
	InjectHook(createfile_orig, createfile_hooked, PATCH_JUMP, false);
	
	if (stricmp(filename, "vehicle.txd") == 0) {
		char szFile[261] = {0};
		wchar_t wszFileName[261] = {0};
		int len = wsprintf(szFile, "%s\\models\\generic\\%s", gamedir, filename);
		size_t converted_chars;
		mbstowcs_s(&converted_chars, wszFileName, szFile, len);

		if (fd != -1)
			CloseHandle((void *)fd);
		
		fd = (int)CreateFileW(wszFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes,
			dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
	}
	return fd;
}

void
run(void)
{
	static bool keystate = false;
	CTimeCycle__Update();

	if (!patched) {
		fix_samp();
		get_values();
		apply_patches();
	}
	if (GetAsyncKeyState(reloadkey) & 0x8000) {
		if (!keystate) {
			keystate = true;
			get_values();
			apply_patches();
		}
	} else {
		keystate = false;
	}
}

void
restore_vehicletxd(void)
{
	createfile_orig = (uint64*)GetProcAddress(GetModuleHandle("kernel32.dll"), "CreateFileA");
	GetCurrentDirectory(sizeof(gamedir), gamedir);

	addrbackup = *createfile_orig;
	InjectHook(createfile_orig, createfile_hooked, PATCH_JUMP, false);
}

int __stdcall 
DllMain(HMODULE hModule, uint32 ul_reason_for_call, void *lpReserved)
{
	switch (ul_reason_for_call) {
	case DLL_PROCESS_ATTACH:
		restore_vehicletxd();
		InjectHook(0x53C0DA, run);
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return 1;
}
