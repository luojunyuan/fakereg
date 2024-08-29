// dllmain.cpp : 定义 DLL 应用程序的入口点。
#include "pch.h"

#include "detours.h"
#include <string>
#include <sstream>
#include <map>
#include <tuple>

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
std::string OssFormatStringNormal(const Args&... args) {
	std::ostringstream oss;
	oss << "[fakereg] ";
	(oss << ... << args);
	return oss.str();
}

template<typename... Args>
std::string OssFormatString(const Args&... args) {
	std::ostringstream oss;
	((oss << args << " + "), ...); // c++17
	std::string result = oss.str();
	return result;
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

void PrintBytesToDebug(LPBYTE lpData, DWORD dataSize) {
	std::ostringstream oss;

	for (DWORD i = 0; i < dataSize; ++i) {
		oss << std::hex << std::uppercase << (int)lpData[i] << " ";
	}

	OutputDebugStringA(oss.str().c_str());
}

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

// NOTE:
// 1. fakereg.ini must be encode with UTF-16 LE 否则 GetPrivateProfileStringW 函数无法正确读取宽字符
// 2. ini 读取值文件夹路径末尾必须以 \ 结尾

std::wstring iniFilePath;
std::map<HKEY, std::string> fakeHKeyMap;
HKEY fakeHKeyBase = reinterpret_cast<HKEY>(0x00000470);  // 用于生成虚假 hKey 的基值
HKEY currentFakeHKey = fakeHKeyBase;

#pragma region Operator to Modify ini file

static std::string InsertWow6432NodePath(std::string& fullPath)
{
	const std::string insertStr = "WOW6432Node\\";

	size_t firstSlashPos = fullPath.find('\\');
	if (firstSlashPos != std::string::npos)
	{
		size_t secondSlashPos = fullPath.find('\\', firstSlashPos + 1);
		if (secondSlashPos != std::string::npos)
		{
			// 在第二个反斜杠后插入 "WOW6432Node"
			fullPath.insert(secondSlashPos + 1, insertStr);
		}
	}

	return fullPath;
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

static std::string AddQuoteString(const std::string& key)
{
	return "\"" + key + "\"";
}
static std::wstring RemoveQuoteWString(std::wstring key)
{
	key.erase(0, 1);
	key.pop_back();
	return key;
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

/// <summary>
/// 交换 ini 与注册表中数据格式
/// </summary>
static DWORD ReadINIValue(std::string section, std::string key, LPBYTE lpData, LPDWORD lpcbData) {
	wchar_t buffer[256];

	auto keyQuote = AddQuoteString(key);

	auto sectionu = ShiftJISToUTF16(section);
	auto keyu = ShiftJISToUTF16(keyQuote);

	auto result = GetPrivateProfileStringW(sectionu.c_str(), keyu.c_str(), L"", buffer, sizeof(buffer) / sizeof(wchar_t), iniFilePath.c_str());

	if (result == 0 && buffer[0] == L'\0')
		return 0;

	auto encodeValue = WideStringToShiftJIS(buffer);

	DWORD type = {};

	if (encodeValue.find("hex:") == 0) {  // hex:00,00,00,00
		type = REG_BINARY;

		auto binaryData = ParseHexData(encodeValue);
		memcpy(lpData, binaryData.data(), binaryData.size());
		*lpcbData = binaryData.size();
	}
	else if (encodeValue.find("dword:") == 0) {  // dword:00000000
		type = REG_DWORD;

		DWORD dwordValue = ParseDwordData(encodeValue);
		memcpy(lpData, &dwordValue, sizeof(DWORD));
		*lpcbData = sizeof(DWORD);
	}
	else {
		type = REG_SZ;

		auto splashValue = NormalizeBackslashes(encodeValue);
		memcpy(lpData, splashValue.c_str(), (splashValue.size() + 1));
		*lpcbData = splashValue.size();
	}

	return type;
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
	std::string fullPath;
	if (hKey == HKEY_LOCAL_MACHINE) {
		fullPath = "HKEY_LOCAL_MACHINE\\" + std::string(lpSubKey);
	}
	else if (hKey == HKEY_CURRENT_USER) {
		fullPath = "HKEY_CURRENT_USER\\" + std::string(lpSubKey);
	}
	else {
		return RealRegOpenKeyExA(hKey, lpSubKey, ulOptions, samDesired, phkResult);
	}

	// read ini file key
	if (!IniSectionExists(fullPath) && !IniSectionExists(InsertWow6432NodePath(fullPath)))
		return RealRegOpenKeyExA(hKey, lpSubKey, ulOptions, samDesired, phkResult);

	// TEST Entry: The original behavior 取消注释以下来测试真实读取注册表的场景
	/*OutputDebugStringA(OssFormatString("FakeRegOpenKeyExA  input", hKey, lpSubKey, ulOptions, samDesired, *phkResult).c_str());
	auto result = RealRegOpenKeyExA(hKey, lpSubKey, ulOptions, samDesired, phkResult);
	OutputDebugStringA(OssFormatString("FakeRegOpenKeyExA output", hKey, lpSubKey, ulOptions, samDesired, *phkResult, result).c_str());
	fakeHKeyMap[*phkResult] = fullPath;
	return result;*/
	// TEST Entry END

	HKEY fakeHKey = currentFakeHKey++;
	fakeHKeyMap[fakeHKey] = fullPath;
	*phkResult = fakeHKey;

	OutputDebugStringA(OssFormatStringNormal("Open fake hKey: ", *phkResult).c_str());

	return ERROR_SUCCESS;
}

LONG WINAPI FakeRegCloseKey(HKEY hKey) {
	if (fakeHKeyMap.find(hKey) == fakeHKeyMap.end())
		return RealRegCloseKey(hKey);

	fakeHKeyMap.erase(hKey);

	OutputDebugStringA(OssFormatStringNormal("Close hooked hKey: 0x", std::hex, std::uppercase, reinterpret_cast<uintptr_t>(hKey)).c_str());
	return ERROR_SUCCESS;
}

// LSTATUS RegQueryValueExA(
//	[in]                HKEY    hKey,
//	[in, optional]      LPCSTR  lpValueName,     // key
//	                    LPDWORD lpReserved,
//	[out, optional]     LPDWORD lpType,          // 传入设置为 NULL，表示不关心
//	[out, optional]     LPBYTE  lpData,          // value 空白说明00
//	[in, out, optional] LPDWORD lpcbData         // 接收值缓冲区大小，一般传入260，会在调用后调整为真实值，如 ".\" -> 3
//);
LONG WINAPI FakeRegQueryValueExA(HKEY hKey, LPCSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData) {
	if (fakeHKeyMap.find(hKey) == fakeHKeyMap.end())
		return RealRegQueryValueExA(hKey, lpValueName, lpReserved, lpType, lpData, lpcbData);

	// Note: Must uncomment TEST Entry first
	/*OutputDebugStringA(OssFormatString("FakeRegQueryValueExA  input", hKey, lpValueName, lpReserved, lpType, lpData, *lpcbData).c_str());
	auto result = RealRegQueryValueExA(hKey, lpValueName, lpReserved, lpType, lpData, lpcbData);
	OutputDebugStringA(OssFormatString("FakeRegQueryValueExA output", hKey, lpValueName, lpReserved, lpType, lpData, *lpcbData, result).c_str());
	return result;*/
	// Note END

	std::string encodeValue = {};

	std::string fullPath = fakeHKeyMap.find(hKey)->second;
	DWORD type = ReadINIValue(fullPath, std::string(lpValueName), lpData, lpcbData); // 不处理  ERROR_MORE_DATA.
	// __KEY_NOT_FOUND__
	if (type == 0)
		return ERROR_FILE_NOT_FOUND;

	if (lpType)
	{
		*lpType = type;
	}

	OutputDebugStringA(OssFormatStringNormal("Query key: ", lpValueName, " value: ", lpData).c_str());

	return ERROR_SUCCESS;
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