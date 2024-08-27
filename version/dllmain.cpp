// dllmain.cpp : 定义 DLL 应用程序的入口点。
#include "pch.h"

#pragma region Forward functions to system version.dll
#pragma comment(linker, "/EXPORT:GetFileVersionInfoA=c:\\windows\\system32\\version.GetFileVersionInfoA")
#pragma comment(linker, "/EXPORT:GetFileVersionInfoByHandle=c:\\windows\\system32\\version.GetFileVersionInfoByHandle")
#pragma comment(linker, "/EXPORT:GetFileVersionInfoExA=c:\\windows\\system32\\version.GetFileVersionInfoExA")
#pragma comment(linker, "/EXPORT:GetFileVersionInfoExW=c:\\windows\\system32\\version.GetFileVersionInfoExW")
#pragma comment(linker, "/EXPORT:GetFileVersionInfoSizeA=c:\\windows\\system32\\version.GetFileVersionInfoSizeA")
#pragma comment(linker, "/EXPORT:GetFileVersionInfoSizeExA=c:\\windows\\system32\\version.GetFileVersionInfoSizeExA")
#pragma comment(linker, "/EXPORT:GetFileVersionInfoSizeExW=c:\\windows\\system32\\version.GetFileVersionInfoSizeExW")
#pragma comment(linker, "/EXPORT:GetFileVersionInfoSizeW=c:\\windows\\system32\\version.GetFileVersionInfoSizeW")
#pragma comment(linker, "/EXPORT:GetFileVersionInfoW=c:\\windows\\system32\\version.GetFileVersionInfoW")
#pragma comment(linker, "/EXPORT:VerFindFileA=c:\\windows\\system32\\version.VerFindFileA")
#pragma comment(linker, "/EXPORT:VerFindFileW=c:\\windows\\system32\\version.VerFindFileW")
#pragma comment(linker, "/EXPORT:VerInstallFileA=c:\\windows\\system32\\version.VerInstallFileA")
#pragma comment(linker, "/EXPORT:VerInstallFileW=c:\\windows\\system32\\version.VerInstallFileW")
#pragma comment(linker, "/EXPORT:VerLanguageNameA=c:\\windows\\system32\\version.VerLanguageNameA")
#pragma comment(linker, "/EXPORT:VerLanguageNameW=c:\\windows\\system32\\version.VerLanguageNameW")
#pragma comment(linker, "/EXPORT:VerQueryValueA=c:\\windows\\system32\\version.VerQueryValueA")
#pragma comment(linker, "/EXPORT:VerQueryValueW=c:\\windows\\system32\\version.VerQueryValueW")
#pragma endregion

static BOOL OpenFileRead(CONST LPCSTR lpFileName, CONST LPHANDLE fileHandle)
{
	OFSTRUCT ofStruct{};
	HFILE hFile = OpenFile(lpFileName, &ofStruct, OF_READ | OF_PROMPT);

	*fileHandle = (HANDLE)(INT_PTR)hFile;  // NOLINT(performance-no-int-to-ptr)
	return hFile != HFILE_ERROR;
}

BOOL APIENTRY DllMain(CONST HMODULE hModule, CONST DWORD fdwReason, CONST LPVOID lpvReserved)
{
	if (fdwReason != DLL_PROCESS_ATTACH)
		return TRUE;

	HANDLE hFile = NULL;
	if (!OpenFileRead("fakereg.dll", &hFile))
	{
		MessageBoxA(NULL, "fakereg.dll not found", "version", MB_ICONERROR);
		return FALSE;
	}
	CloseHandle(hFile);

	if (!LoadLibraryA("fakereg.dll"))
	{
		MessageBoxA(NULL, "fakereg.dll", "version - Failed to load library", MB_ICONERROR);
	}

	return TRUE;
}