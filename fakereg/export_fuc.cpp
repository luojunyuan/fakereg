#include "pch.h"

#include <windows.h>
#include <shellapi.h>

extern "C" __declspec(dllexport) HINSTANCE WINAPI F0(
	HWND hwnd,
	LPCSTR lpOperation,
	LPCSTR lpFile,
	LPCSTR lpParameters,
	LPCSTR lpDirectory,
	INT nShowCmd
) {
	return ShellExecuteA(hwnd, lpOperation, lpFile, lpParameters, lpDirectory, nShowCmd);
}

extern "C" __declspec(dllexport) HINSTANCE WINAPI F1() {
	return (HINSTANCE)DragFinish;
}

extern "C" __declspec(dllexport) HINSTANCE WINAPI F2() {
	return (HINSTANCE)DragQueryFileA;
}