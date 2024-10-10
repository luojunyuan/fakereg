#include "pch.h"

#include <windows.h>
#include <shellapi.h>

//extern "C" __declspec(dllexport) HINSTANCE WINAPI F0() {
//	return (HINSTANCE)ShellExecuteA;
//}
extern "C"  __declspec(dllexport, naked) void WINAPI F0() {
	__asm {
		jmp ShellExecuteA
	}
}

extern "C" __declspec(dllexport) HINSTANCE WINAPI F1() {
	return (HINSTANCE)DragFinish;
}

extern "C" __declspec(dllexport) HINSTANCE WINAPI F2() {
	return (HINSTANCE)DragQueryFileA;
}

extern "C" __declspec(dllexport, naked) void WINAPI F3() {
	__asm {
		jmp SHAppBarMessage
	}
}

//extern "C" __declspec(dllexport) HINSTANCE WINAPI F4() {
//	return (HINSTANCE)DirectInput8Create;
//}