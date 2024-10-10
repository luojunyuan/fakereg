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
//		if (*a2) { // ���ָ��ָ��ĵ�һ���ַ��Ƿ�Ϊ�ǿ�
//			do {
//				++result; // ͳ�Ʒǿ��ַ�
//			} while (a2[result] != '\0'); // �������ַ�����
//		}
//		return result;
//	}
//
//	int ConvertUTF8ToUTF16(int output, const char* a2) {
//		int v2 = strlen(a2);  // ���������ֽ�����ĳ���
//		int v3 = 0;
//		int v13 = v2;         // ���泤��
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
//				// ����ַ��� ASCII�����ֽ��ַ���
//				if (v8 >= 0) {
//					*reinterpret_cast<uint16_t*>(output + 2 * v14) = static_cast<unsigned char>(v5[v4]);
//					v3 = v14 + 1;
//					++v14;
//					goto LABEL_14;
//				}
//
//				// �����Ҫ������ֽ��ַ�
//				if (v7 < v2) {
//					// ��������ֽ��ַ� (110xxxxx 10xxxxxx)
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
//				// ��������ֽ��ַ� (1110xxxx 10xxxxxx 10xxxxxx)
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
//		// ������� UTF-16 �ַ���ĩβ�����ֹ��
//		*reinterpret_cast<uint16_t*>(output + 2 * v3) = 0;
//		return 0;
//	}
//
//	int GetWideStringLength(const wchar_t* a1) {
//		int result = 0;  // ��ʼ������Ϊ0
//		if (*a1) {  // ���ָ��ĵ�һ���ַ��ǿ�
//			do {
//				++result;  // ����ÿ���ַ�
//			} while (a1[result] != L'\0');  // ֱ���������ַ��Ŀ�ֵ (L'\0')
//		}
//		return result;  // ���ؿ��ַ����ĳ���
//	}
//
//	int CopyWideString(int a1, const __int16* a2) {
//		int v2 = 0;  // �����������ڼ�¼�ַ�������
//		__int16 v3 = *a2;  // ��ȡ�����ַ����ĵ�һ���ַ�
//
//		if (v3) {  // �������ĵ�һ���ַ����ǿ��ַ�
//			int v4 = 0;  // ƫ���������ڼ���Ŀ���ַ
//			do {
//				++v2;  // ����
//				*reinterpret_cast<__int16*>(a1 + v4) = v3;  // �����ַ���Ŀ���ַ
//				v4 = 2 * v2;  // ����ƫ������ָ����һ��λ�� (��Ϊÿ���ַ��� 2 �ֽ�)
//				v3 = a2[v2];  // ��ȡ��һ���ַ�
//			} while (v3);  // ѭ��ֱ���������ַ�
//		}
//
//		// ��Ŀ���ַ���ĩβ���һ�����ַ�
//		*reinterpret_cast<__int16*>(a1 + 2 * v2) = 0;
//		return 0;  // ���� 0 ��ʾ�ɹ�
//	}
//
//	// -------------------
//
//	int CreateOrOpenFile(char a2) {
//		DWORD accessFlags = 0;
//
//		// ���� a2 ���÷���Ȩ��
//		if (a2 & 1) {
//			accessFlags = GENERIC_READ;  // 0x80000000
//		}
//		if (a2 & 2) {
//			accessFlags |= GENERIC_WRITE;  // 0x40000000
//		}
//
//		// �� thisPtr + 4 ��ȡ�ļ���
//		LPCWSTR fileName = *(LPCWSTR*)((char*)this + 4);
//
//		// ���� CreateFileW �򿪻򴴽��ļ�
//		HANDLE fileHandle = CreateFileW(
//			fileName,            // �ļ���
//			accessFlags,         // ����Ȩ��
//			FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,  // ����ģʽ
//			nullptr,             // ��ȫ����
//			(a2 & 4) ? OPEN_ALWAYS : CREATE_ALWAYS,  // ��� a2 & 4 Ϊ�棬ѡ�� OPEN_ALWAYS������ѡ�� CREATE_ALWAYS
//			FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,  // �ļ����Ժͱ�־
//			nullptr);            // ģ���ļ����
//
//		// ������洢�� thisPtr + 8
//		*(HANDLE*)((char*)this + 8) = fileHandle;
//
//		// �ж��ļ�����Ƿ���Ч
//		return (fileHandle != INVALID_HANDLE_VALUE) ? 0 : -1;
//	}
//};