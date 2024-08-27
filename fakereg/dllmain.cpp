// dllmain.cpp : 定义 DLL 应用程序的入口点。
#include "pch.h"

#include "detours.h"
#include <string>
#include <sstream>

#include <map>
#include <vector>

#pragma region String Converter

static std::wstring ShiftJISToUTF16(const std::string& shiftJisStr) {
	int len = MultiByteToWideChar(932, 0, shiftJisStr.c_str(), -1, NULL, 0);
	std::wstring wideStr(len, L'\0');
	MultiByteToWideChar(932, 0, shiftJisStr.c_str(), -1, &wideStr[0], len);
	return wideStr;
}

static std::string WideStringToShiftJIS(const std::wstring& wideString) {
	int size_needed = WideCharToMultiByte(932, 0, wideString.c_str(), -1, NULL, 0, NULL, NULL);
	std::string shiftjis(size_needed, 0);
	WideCharToMultiByte(932, 0, wideString.c_str(), -1, &shiftjis[0], size_needed, NULL, NULL);
	return shiftjis;
}

template<typename... Args>
std::string OssFormatString(const Args&... args) {
	std::ostringstream oss;
	(oss << ... << args); //c++17
	return oss.str();
}

/// <summary>
/// 为了在中文系统的 DebugView 内打印日文字符
/// </summary>
/// <param name="shiftJisStr"></param>
/// <returns></returns>
static std::string ConvertShiftJISToGBK(const std::string& shiftJisStr) {
	std::wstring utf16Str = ShiftJISToUTF16(shiftJisStr);

	int len = WideCharToMultiByte(936, 0, utf16Str.c_str(), -1, NULL, 0, NULL, NULL);
	std::string gbkStr(len, '\0');
	WideCharToMultiByte(936, 0, utf16Str.c_str(), -1, &gbkStr[0], len, NULL, NULL);
	return gbkStr;
}

// TODO 1: 要达到打印调用前的函数参数 字符串或 byte，打印调用后函数参数 对比的效果。
// for debug use
//static void OutputDebugStringWithBytesA(LPCSTR str) {
//	std::ostringstream oss;
//	int i = 0;
//
//	while (str[i] != '\0') {  // 遍历直到遇到字符串结束符 '\0'
//		oss << std::setw(2) << std::setfill('0') << std::hex << (unsigned int)(unsigned char)str[i] << " ";
//		i++;
//	}
//
//	std::string hexStr = oss.str();
//	OutputDebugStringA(hexStr.c_str());
//}

#pragma endregion

// 原始函数指针
decltype(&RegOpenKeyExA) RealRegOpenKeyExA = RegOpenKeyExA;
decltype(&RegCloseKey) RealRegCloseKey = RegCloseKey;
decltype(&RegQueryValueExA) RealRegQueryValueExA = RegQueryValueExA;

decltype(&RegFlushKey) RealRegFlushKey = RegFlushKey;
decltype(&RegSetValueExA) RealRegSetValueExA = RegSetValueExA;
decltype(&RegCreateKeyExA) RealRegCreateKeyExA = RegCreateKeyExA;
decltype(&RegEnumValueA) RealRegEnumValueA = RegEnumValueA;

decltype(&RegOpenKeyA) RealRegOpenKeyA = RegOpenKeyA;

// NOTE: fakereg.ini must be encode with UTF-16 LE 否则 GetPrivateProfileStringW 函数无法正确读取宽字符
std::wstring iniFilePath;
std::map<HKEY, std::string> fakeHKeyMap;
HKEY fakeHKeyBase = reinterpret_cast<HKEY>(0xDEADBEEF);  // 用于生成虚假 hKey 的基值
HKEY currentFakeHKey = fakeHKeyBase;

#pragma region Operator to Modify ini file

static std::string AddQuoteString(std::string key)
{
	return "\"" + std::move(key) + "\"";
}
static std::wstring RemoveQuoteWString(std::wstring key)
{
	key.erase(0, 1);
	key.pop_back();
	return key;
}
static std::string NormalizeBackslashes(const std::string& input) {
	std::string output;
	output.reserve(input.size());

	for (size_t i = 0; i < input.size(); ++i) {
		if (input[i] == '\\' && i + 1 < input.size() && input[i + 1] == '\\') {
			output.push_back('\\');
			++i;
		}
		else {
			output.push_back(input[i]);
		}
	}

	return output;
}

static bool IniSectionExists(std::string sectionName) {
	wchar_t buffer[256];

	auto fullPathu = ShiftJISToUTF16(sectionName);

	// 尝试读取该 section 中的第一个 key。如果 section 不存在，则 buffer 中会返回默认值。
	DWORD charsRead = GetPrivateProfileStringW(
		fullPathu.c_str(), NULL, NULL, buffer, sizeof(buffer) / sizeof(wchar_t),
		iniFilePath.c_str());

	return charsRead > 0;
}

static std::string ReadINIValue(std::string path, std::string key) {
	wchar_t buffer[256];

	auto keyQuote = AddQuoteString(key);

	auto pathu = ShiftJISToUTF16(path);
	auto keyu = ShiftJISToUTF16(keyQuote);

	auto result = GetPrivateProfileStringW(pathu.c_str(), keyu.c_str(), L"", buffer, sizeof(buffer) / sizeof(wchar_t), iniFilePath.c_str());

	auto shiftjis = WideStringToShiftJIS(buffer);

	// TODO 2: 理应确定是字符串或者path后再处理，考虑type怎么处理
	return NormalizeBackslashes(shiftjis);
}

static std::string GetNthKeyValueInSection(const std::string& sectionName, int n) {
	wchar_t keyBuffer[1024];

	auto secu = ShiftJISToUTF16(sectionName);

	// 获取指定 section 下的所有键名
	DWORD charsRead = GetPrivateProfileStringW(
		secu.c_str(),
		NULL,
		NULL,
		keyBuffer,
		sizeof(keyBuffer) / sizeof(wchar_t),
		iniFilePath.c_str()
	);

	if (charsRead == 0) {
		return "";
	}

	wchar_t* key = keyBuffer;
	int currentIndex = 0;
	while (*key) {
		if (currentIndex == n) {
			auto nkey = RemoveQuoteWString(key);
			return WideStringToShiftJIS(nkey);
		}

		key += wcslen(key) + 1;
		currentIndex++;
	}

	return "";
}

#pragma endregion

LONG WINAPI FakeRegOpenKeyExA(HKEY hKey, LPCSTR lpSubKey, DWORD ulOptions, REGSAM samDesired, PHKEY phkResult) {
	std::string baseKey;
	if (hKey == HKEY_LOCAL_MACHINE) {
		baseKey = "HKEY_LOCAL_MACHINE\\";
	}
	else if (hKey == HKEY_CURRENT_USER) {
		baseKey = "HKEY_CURRENT_USER\\";
	}
	else {
		return RealRegOpenKeyExA(hKey, lpSubKey, ulOptions, samDesired, phkResult);
	}

	auto fullPath = baseKey + lpSubKey;

	// read ini file key
	if (!IniSectionExists(fullPath))
		return RealRegOpenKeyExA(hKey, lpSubKey, ulOptions, samDesired, phkResult);

	HKEY fakeHKey = currentFakeHKey++;
	fakeHKeyMap[fakeHKey] = fullPath;
	*phkResult = fakeHKey;

	// TEST: The original behavior 取消注释以下两行来测试真实读取注册表的情况
	//auto result = RealRegOpenKeyExA(hKey, lpSubKey, ulOptions, samDesired, phkResult);
	//fakeHKeyMap[*phkResult] = fullPath;

	std::string message = OssFormatString("Open fake hKey ", *phkResult);
	OutputDebugStringA(message.c_str());

	return ERROR_SUCCESS;
}

LONG WINAPI FakeRegCloseKey(HKEY hKey) {
	if (fakeHKeyMap.find(hKey) == fakeHKeyMap.end())
		return RealRegCloseKey(hKey);

	fakeHKeyMap.erase(hKey);

	std::string message = OssFormatString("Close hooked hKey: 0x", std::hex, std::uppercase, reinterpret_cast<uintptr_t>(hKey));
	OutputDebugStringA(message.c_str());
	return ERROR_SUCCESS;
}

static std::vector<BYTE> ParseHexData(const std::string& hexString) {
	std::vector<BYTE> data;
	size_t pos = 4; // 跳过 "hex:" 前缀

	while (pos < hexString.size()) {
		// 找到下一个逗号或字符串结束
		size_t end = hexString.find(',', pos);
		if (end == std::wstring::npos) {
			end = hexString.size();
		}

		// 提取并转换当前的十六进制字符串部分
		std::string byteString = hexString.substr(pos, end - pos);
		BYTE byteValue = static_cast<BYTE>(std::strtol(byteString.c_str(), nullptr, 16));
		data.push_back(byteValue);

		pos = end + 1;  // 移动到下一个字符
	}

	return data;
}

DWORD ParseDwordData(const std::string& dwordString) {
	return std::stoul(dwordString.substr(6), nullptr, 16); // Skip "dword:" and convert
}

LONG WINAPI FakeRegQueryValueExA(HKEY hKey, LPCSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData) {
	auto it = fakeHKeyMap.find(hKey);
	if (it == fakeHKeyMap.end())
		return RealRegQueryValueExA(hKey, lpValueName, lpReserved, lpType, lpData, lpcbData);

	// TEST: The original behavior
	// Value before called
	//auto result = RealRegQueryValueExA(hKey, lpValueName, lpReserved, lpType, lpData, lpcbData);
	// Value after called
	//OutputDebugStringA(ss.str().c_str());
	//return result;

	std::string fullPath = it->second;
	std::string ival = ReadINIValue(fullPath, std::string(lpValueName));

	if (!ival.empty()) {
		std::string message = OssFormatString("Query key: ", lpValueName, " result: ", ival.c_str());
		OutputDebugStringA(message.c_str());

		// TODO 2: 考虑type怎么处理
		if (ival.find("hex:") == 0) {  // 处理 REG_BINARY 数据 hex:00,00,00,00
			auto binaryData = ParseHexData(ival);

			if (lpData && lpcbData && *lpcbData >= binaryData.size()) {
				memcpy(lpData, binaryData.data(), binaryData.size());
				*lpcbData = binaryData.size();

				if (lpType) {
					*lpType = REG_BINARY;
				}
				return ERROR_SUCCESS;
			}
			else {
				return ERROR_MORE_DATA;  // 缓冲区太小
			}
		}
		if (ival.find("dword:") == 0) {  // 处理 REG_DWORD 数据 dword:00000000
			DWORD dwordValue = ParseDwordData(ival);

			if (lpData && lpcbData && *lpcbData >= sizeof(DWORD)) {
				memcpy(lpData, &dwordValue, sizeof(DWORD));
				*lpcbData = sizeof(DWORD);

				if (lpType) {
					*lpType = REG_DWORD;
				}
				return ERROR_SUCCESS;
			}
			else {
				*lpcbData = sizeof(DWORD);
				return ERROR_MORE_DATA;  // 缓冲区太小
			}
		}
		else {  // 处理字符串数据
			if (lpData && lpcbData && *lpcbData >= (ival.size() + 1)) {  // 注意这里不需要乘以 sizeof(wchar_t)，因为是 std::string
				memcpy(lpData, ival.c_str(), (ival.size() + 1));  // 复制 std::string 数据，包括终止符
				*lpcbData = (ival.size() + 1);

				if (lpType) {
					*lpType = REG_SZ;
				}
				return ERROR_SUCCESS;
			}
			else {
				return ERROR_MORE_DATA;  // 缓冲区太小
			}
		}
	}
}

// TODO 5:
LONG WINAPI FakeRegSetValueExA(HKEY hKey, LPCSTR lpValueName, DWORD Reserved, DWORD dwType, const BYTE* lpData, DWORD cbData) {
	OutputDebugString(L"-Call FakeRegSetValueExA");
	return RealRegSetValueExA(hKey, lpValueName, Reserved, dwType, lpData, cbData);
}
// 如果游戏写注册表很可能会Flush
LONG WINAPI FakeRegFlushKey(HKEY hKey) {
	if (fakeHKeyMap.find(hKey) == fakeHKeyMap.end())
		return RealRegFlushKey(hKey);

	std::string message = OssFormatString("Flush fake hKey: ", hKey);
	OutputDebugStringA(message.c_str());
	return ERROR_SUCCESS;
}

LONG WINAPI FakeRegCreateKeyExA(HKEY hKey, LPCSTR lpSubKey, DWORD Reserved, LPSTR lpClass, DWORD dwOptions, REGSAM samDesired, LPSECURITY_ATTRIBUTES lpSecurityAttributes, PHKEY phkResult, LPDWORD lpdwDisposition) {
	OutputDebugString(L"-Call FakeRegCreateKeyExA");
	return RealRegCreateKeyExA(hKey, lpSubKey, Reserved, lpClass, dwOptions, samDesired, lpSecurityAttributes, phkResult, lpdwDisposition);
}

bool ConvertStringToLPSTR(const std::string& str, LPSTR lpValueName, LPDWORD lpcchValueName) {
	// 确定所需的缓冲区大小（以字节为单位），包括终止的 '\0'
	int requiredSize = static_cast<int>(str.size() + 1);

	// 检查提供的缓冲区是否足够大
	if (requiredSize > *lpcchValueName) {
		// 如果缓冲区太小，返回所需的大小并返回错误
		*lpcchValueName = requiredSize;
		return false;  // 提供的缓冲区太小
	}

	// 进行字符串复制
	memcpy(lpValueName, str.c_str(), requiredSize);

	// 返回实际写入的字符数
	*lpcchValueName = requiredSize;
	return true;
}

LONG WINAPI FakeRegEnumValueA(HKEY hKey, DWORD dwIndex, LPSTR lpValueName, LPDWORD lpcchValueName, LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData) {
	if (fakeHKeyMap.find(hKey) == fakeHKeyMap.end())
		return RealRegEnumValueA(hKey, dwIndex, lpValueName, lpcchValueName, lpReserved, lpType, lpData, lpcbData);

	std::string sec = fakeHKeyMap.find(hKey)->second;
	std::string key = GetNthKeyValueInSection(sec, dwIndex);
	if (key.empty())
		return ERROR_NO_MORE_ITEMS;

	// TODO 3: 这里比较奇怪
	auto convertResult = ConvertStringToLPSTR(key, lpValueName, lpcchValueName);

	// deal with type, if lpType is Null ?
	//wchar_t valueBuffer[256];
	//GetPrivateProfileStringW(sec.c_str(), key.c_str(), L"", valueBuffer, sizeof(valueBuffer) / sizeof(wchar_t), iniFilePath.c_str());
	//if (wcsncmp(valueBuffer, L"hex:", 4) == 0) {
	//	*lpType = REG_BINARY;
	//}
	//else {
	//	*lpType = REG_SZ;
	//}
	// TODO 4: 把1实现然后仔细观察完善真实情况，理解每个函数用处

	std::string message = OssFormatString("Enum fake hKey ", hKey, " with ", lpValueName, "(", *lpcchValueName, ")");
	OutputDebugStringA(message.c_str());

	return ERROR_SUCCESS;
}

LONG WINAPI FakeRegOpenKeyA(HKEY hKey, LPCSTR lpSubKey, PHKEY phkResult) {
	return FakeRegOpenKeyExA(hKey, lpSubKey, 0, KEY_READ, phkResult);
}

static std::wstring GetExecutablePath() {
	wchar_t buffer[MAX_PATH];
	GetModuleFileNameW(NULL, buffer, MAX_PATH);
	std::wstring::size_type pos = std::wstring(buffer).find_last_of(L"\\/");
	return std::wstring(buffer).substr(0, pos);
}

BOOL APIENTRY DllMain(HMODULE hModule,
	DWORD  ul_reason_for_call,
	LPVOID lpReserved) {
	switch (ul_reason_for_call) {
	case DLL_PROCESS_ATTACH:
		iniFilePath = GetExecutablePath() + L"\\fakereg.ini";
		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		// 游戏所需要的基本读取
		DetourAttach(&(PVOID&)RealRegOpenKeyExA, FakeRegOpenKeyExA);
		DetourAttach(&(PVOID&)RealRegCloseKey, FakeRegCloseKey);
		DetourAttach(&(PVOID&)RealRegQueryValueExA, FakeRegQueryValueExA);
		// 部分游戏会轮询一个HKEY
		DetourAttach(&(PVOID&)RealRegEnumValueA, FakeRegEnumValueA);
		// 部分游戏会写入到REG
		DetourAttach(&(PVOID&)RealRegSetValueExA, FakeRegSetValueExA);
		DetourAttach(&(PVOID&)RealRegFlushKey, FakeRegFlushKey);

		DetourAttach(&(PVOID&)RealRegCreateKeyExA, FakeRegCreateKeyExA);
		DetourAttach(&(PVOID&)RealRegOpenKeyA, FakeRegOpenKeyA);
		DetourTransactionCommit();
		break;

	case DLL_PROCESS_DETACH:
		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		DetourDetach(&(PVOID&)RealRegOpenKeyExA, FakeRegOpenKeyExA);
		DetourDetach(&(PVOID&)RealRegCloseKey, FakeRegCloseKey);
		DetourDetach(&(PVOID&)RealRegQueryValueExA, FakeRegQueryValueExA);
		DetourDetach(&(PVOID&)RealRegEnumValueA, FakeRegEnumValueA);
		DetourDetach(&(PVOID&)RealRegSetValueExA, FakeRegSetValueExA);
		DetourDetach(&(PVOID&)RealRegFlushKey, FakeRegFlushKey);
		DetourDetach(&(PVOID&)RealRegCreateKeyExA, FakeRegCreateKeyExA);
		DetourDetach(&(PVOID&)RealRegOpenKeyA, FakeRegOpenKeyA);
		DetourTransactionCommit();
		break;
	}
	return TRUE;
}