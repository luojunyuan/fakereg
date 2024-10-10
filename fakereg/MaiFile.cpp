#include "pch.h"
//
//#include <cstdint> // uint16_t
//
//class MaiFile
//{
//public:
//	MaiFile(char* a2) // 4096 increase
//	{
//		//auto a2 = operator new(0x14u);
//
//		InitializeHeapHandle(&heapHandle);
//		auto v2 = GetByteArrayLength(a2);
//		auto block = reinterpret_cast<int>(AllocLpFileName(2 * v2 + 2));
//		ConvertUTF8ToUTF16(block, a2);
//		auto wLength = GetWideStringLength(block);
//		auto newBlock = AllocLpFileName(2 * wLength + 2);
//		this[1] = newBlock;
//		CopyWideString(newBlock, block); // copy block to new block
//		this[2] = -1;
//		Release(block);
//	}
//	~MaiFile();
//
//private:
//	// this virtual table
//	HANDLE heapHandle = nullptr;  // this + 3
//	HANDLE handle8 = nullptr; // this + 4
//
//	HANDLE* InitializeHeapHandle(HANDLE* handle) {
//		*handle = GetProcessHeap();
//		handle8 = (HANDLE)8;
//		return handle;
//	}
//
//	LPVOID AllocLpFileName(SIZE_T dwBytes)
//	{
//		return HeapAlloc(*(HANDLE*)this, *(DWORD*)(this + 4), dwBytes);
//	}
//	int Release(LPVOID lpMem)
//	{
//		return HeapFree(*(HANDLE*)this, (*(BYTE*)(this + 4) & 1) != 0, lpMem) - 1;
//	}
//
//	int GetByteArrayLength(const char* a2) {
//		int result = 0;
//		if (*a2) { // 检查指针指向的第一个字符是否为非空
//			do {
//				++result; // 统计非空字符
//			} while (a2[result] != '\0'); // 遇到空字符结束
//		}
//		return result;
//	}
//
//	int ConvertUTF8ToUTF16(int output, const char* a2) {
//		int v2 = strlen(a2);  // 计算输入字节数组的长度
//		int v3 = 0;
//		int v13 = v2;         // 保存长度
//		int v14 = 0;
//		int v4 = 0;
//
//		if (v2 > 0) {
//			const char* v5 = a2;
//			int v6 = 2;
//			int v7 = 1;
//
//			do {
//				char v8 = v5[v4];
//
//				// 如果字符是 ASCII（单字节字符）
//				if (v8 >= 0) {
//					*reinterpret_cast<uint16_t*>(output + 2 * v14) = static_cast<unsigned char>(v5[v4]);
//					v3 = v14 + 1;
//					++v14;
//					goto LABEL_14;
//				}
//
//				// 如果需要处理多字节字符
//				if (v7 < v2) {
//					// 如果是两字节字符 (110xxxxx 10xxxxxx)
//					if ((v8 & 0xE0) == 0xC0) {
//						char v9 = v5[v4 + 1] & 0x3F;
//						v4 += 2;
//						v7 += 2;
//						*reinterpret_cast<uint16_t*>(output + 2 * v14) = ((v8 & 0x1F) << 6) | v9;
//						v3 = v14 + 1;
//						v6 += 2;
//						++v14;
//						continue;
//					}
//				}
//
//				// 如果是三字节字符 (1110xxxx 10xxxxxx 10xxxxxx)
//				if (v6 < v2 && (v8 & 0xF0) == 0xE0) {
//					v7 += 3;
//					uint16_t v10 = (v8 << 6) | (v5[v4 + 1] & 0x3F);
//					char v11 = v5[v4 + 2];
//					v4 += 3;
//					*reinterpret_cast<uint16_t*>(output + 2 * v14) = (v10 << 6) | (v11 & 0x3F);
//					v3 = v14 + 1;
//					v6 += 3;
//					++v14;
//					continue;
//				}
//
//			LABEL_13:
//				v3 = v14;
//			LABEL_14:
//				++v4;
//				++v7;
//				++v6;
//			} while (v4 < v2);
//		}
//
//		// 在输出的 UTF-16 字符串末尾添加终止符
//		*reinterpret_cast<uint16_t*>(output + 2 * v3) = 0;
//		return 0;
//	}
//
//	int GetWideStringLength(const wchar_t* a1) {
//		int result = 0;  // 初始化长度为0
//		if (*a1) {  // 如果指向的第一个字符非空
//			do {
//				++result;  // 计数每个字符
//			} while (a1[result] != L'\0');  // 直到遇到宽字符的空值 (L'\0')
//		}
//		return result;  // 返回宽字符串的长度
//	}
//
//	int CopyWideString(int a1, const __int16* a2) {
//		int v2 = 0;  // 计数器，用于记录字符的数量
//		__int16 v3 = *a2;  // 获取输入字符串的第一个字符
//
//		if (v3) {  // 如果输入的第一个字符不是空字符
//			int v4 = 0;  // 偏移量，用于计算目标地址
//			do {
//				++v2;  // 计数
//				*reinterpret_cast<__int16*>(a1 + v4) = v3;  // 复制字符到目标地址
//				v4 = 2 * v2;  // 更新偏移量，指向下一个位置 (因为每个字符是 2 字节)
//				v3 = a2[v2];  // 获取下一个字符
//			} while (v3);  // 循环直到遇到空字符
//		}
//
//		// 在目标字符串末尾添加一个空字符
//		*reinterpret_cast<__int16*>(a1 + 2 * v2) = 0;
//		return 0;  // 返回 0 表示成功
//	}
//
//	// -------------------
//
//	int CreateOrOpenFile(char a2) {
//		DWORD accessFlags = 0;
//
//		// 根据 a2 设置访问权限
//		if (a2 & 1) {
//			accessFlags = GENERIC_READ;  // 0x80000000
//		}
//		if (a2 & 2) {
//			accessFlags |= GENERIC_WRITE;  // 0x40000000
//		}
//
//		// 从 thisPtr + 4 获取文件名
//		LPCWSTR fileName = *(LPCWSTR*)((char*)this + 4);
//
//		// 调用 CreateFileW 打开或创建文件
//		HANDLE fileHandle = CreateFileW(
//			fileName,            // 文件名
//			accessFlags,         // 访问权限
//			FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,  // 共享模式
//			nullptr,             // 安全属性
//			(a2 & 4) ? OPEN_ALWAYS : CREATE_ALWAYS,  // 如果 a2 & 4 为真，选择 OPEN_ALWAYS，否则选择 CREATE_ALWAYS
//			FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,  // 文件属性和标志
//			nullptr);            // 模板文件句柄
//
//		// 将句柄存储到 thisPtr + 8
//		*(HANDLE*)((char*)this + 8) = fileHandle;
//
//		// 判断文件句柄是否有效
//		return (fileHandle != INVALID_HANDLE_VALUE) ? 0 : -1;
//	}
//};