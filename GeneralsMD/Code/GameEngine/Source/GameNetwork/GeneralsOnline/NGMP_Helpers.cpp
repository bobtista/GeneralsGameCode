#include "GameNetwork/GeneralsOnline/NGMP_include.h"
#include <chrono>
#include <cwctype>
#include <ctime>
#include <mutex>
#include <string>
#include "../OnlineServices_Init.h"
#include "../OnlineServices_Auth.h"

std::string m_strNetworkLogFileName;
std::mutex m_logMutex;

extern NGMPGame* TheNGMPGame;

#if !defined(_WIN32)
// wchar_t is UTF-32 here. Invalid input becomes U+FFFD, matching the Win32 conversions below.
std::string to_utf8(const std::wstring& wstr)
{
	std::string result;
	result.reserve(wstr.size());
	for (wchar_t wc : wstr)
	{
		uint32_t cp = (uint32_t)wc;
		if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
			cp = 0xFFFD;

		if (cp < 0x80)
		{
			result += (char)cp;
		}
		else if (cp < 0x800)
		{
			result += (char)(0xC0 | (cp >> 6));
			result += (char)(0x80 | (cp & 0x3F));
		}
		else if (cp < 0x10000)
		{
			result += (char)(0xE0 | (cp >> 12));
			result += (char)(0x80 | ((cp >> 6) & 0x3F));
			result += (char)(0x80 | (cp & 0x3F));
		}
		else
		{
			result += (char)(0xF0 | (cp >> 18));
			result += (char)(0x80 | ((cp >> 12) & 0x3F));
			result += (char)(0x80 | ((cp >> 6) & 0x3F));
			result += (char)(0x80 | (cp & 0x3F));
		}
	}
	return result;
}

std::wstring from_utf8(const std::string& utf8_str)
{
	std::wstring result;
	result.reserve(utf8_str.size());
	const unsigned char* p = (const unsigned char*)utf8_str.data();
	const size_t n = utf8_str.size();
	size_t i = 0;
	while (i < n)
	{
		unsigned char c = p[i];
		uint32_t cp = 0;
		size_t extra = 0;
		uint32_t minValue = 0;
		if (c < 0x80) { cp = c; extra = 0; minValue = 0; }
		else if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; extra = 1; minValue = 0x80; }
		else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; extra = 2; minValue = 0x800; }
		else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; extra = 3; minValue = 0x10000; }
		else
		{
			result += (wchar_t)0xFFFD;
			++i;
			continue;
		}

		size_t j = 1;
		for (; j <= extra; ++j)
		{
			if (i + j >= n || (p[i + j] & 0xC0) != 0x80)
				break;
			cp = (cp << 6) | (p[i + j] & 0x3F);
		}

		if (j <= extra || cp < minValue || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
		{
			result += (wchar_t)0xFFFD;
			i += j;
			continue;
		}

		result += (wchar_t)cp;
		i += extra + 1;
	}
	return result;
}
#else
// Win32 conversions handle surrogate pairs and substitute U+FFFD for invalid input instead of throwing,
// so a malformed name from the server can't take down the UI
std::string to_utf8(const std::wstring& wstr)
{
	if (wstr.empty())
		return std::string();

	int len = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), (int)wstr.size(), nullptr, 0, nullptr, nullptr);
	std::string result(len, '\0');
	WideCharToMultiByte(CP_UTF8, 0, wstr.data(), (int)wstr.size(), result.data(), len, nullptr, nullptr);
	return result;
}

std::wstring from_utf8(const std::string& utf8_str)
{
	if (utf8_str.empty())
		return std::wstring();

	int len = MultiByteToWideChar(CP_UTF8, 0, utf8_str.data(), (int)utf8_str.size(), nullptr, 0);
	std::wstring result(len, L'\0');
	MultiByteToWideChar(CP_UTF8, 0, utf8_str.data(), (int)utf8_str.size(), result.data(), len);
	return result;
}
#endif

#ifndef WC_NO_BEST_FIT_CHARS
#define WC_NO_BEST_FIT_CHARS 0x00000400
#endif

// Process code page text (paths from local file APIs) to UTF-8 for the wire
std::string local_to_utf8(const std::string& local_str)
{
	if (local_str.empty() || GetACP() == CP_UTF8)
		return local_str;

	int wlen = MultiByteToWideChar(CP_ACP, 0, local_str.data(), (int)local_str.size(), nullptr, 0);
	if (wlen <= 0)
		return local_str;

	std::wstring wide(wlen, L'\0');
	MultiByteToWideChar(CP_ACP, 0, local_str.data(), (int)local_str.size(), wide.data(), wlen);
	return to_utf8(wide);
}

// UTF-8 from the wire to the process code page that local file APIs expect; unrepresentable characters become '_'
std::string utf8_to_local(const std::string& utf8_str)
{
	if (utf8_str.empty() || GetACP() == CP_UTF8)
		return utf8_str;

	std::wstring wide = from_utf8(utf8_str);
	if (wide.empty())
		return std::string();

	int len = WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, wide.data(), (int)wide.size(), nullptr, 0, "_", nullptr);
	if (len <= 0)
		return utf8_str;

	std::string result(len, '\0');
	WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, wide.data(), (int)wide.size(), result.data(), len, "_", nullptr);
	return result;
}

// Legacy format strings take player names as narrow %hs arguments, which get widened byte by byte and mangle UTF-8.
// Rewriting %hs to %s lets callers pass a from_utf8() decoded name instead.
std::wstring WidenFormatSpecifiers(const std::wstring& format)
{
	std::wstring widened = format;
	for (size_t pos = widened.find(L"%hs"); pos != std::wstring::npos; pos = widened.find(L"%hs", pos + 2))
	{
		widened.erase(pos + 1, 1);
	}
	return widened;
}

std::wstring NormalizeSingleLineText(const std::wstring& text)
{
	std::wstring normalizedText;
	normalizedText.reserve(text.size());
	bool pendingSpace = false;

	for (wchar_t character : text)
	{
		if (std::iswspace(character) || std::iswcntrl(character))
		{
			pendingSpace = !normalizedText.empty();
			continue;
		}

		if (pendingSpace)
		{
			normalizedText += L' ';
			pendingSpace = false;
		}
		normalizedText += character;
	}

	return normalizedText;
}

void NetworkLog(ELogVerbosity logVerbosity, const char* fmt, ...)
{
	if (!NGMP_OnlineServicesManager::Settings.Debug_VerboseLogging())
	{
		if (logVerbosity < g_LogVerbosity)
		{
			return;
		}
	}

	std::scoped_lock scopedLock { m_logMutex };

	if (m_strNetworkLogFileName.empty())
	{
		auto now = std::chrono::system_clock::now();
		auto in_time_t = std::chrono::system_clock::to_time_t(now);

#if defined(_DEBUG)
		// for debug, put the user ID in the log name so we can easily track it (NOte that this does mean we dont get very early logging, pre login, but that's OK normally)

		NGMP_OnlineServices_AuthInterface* pAuthInterface = NGMP_OnlineServicesManager::GetInterface<NGMP_OnlineServices_AuthInterface>();
		if (pAuthInterface != nullptr && pAuthInterface->GetUserID() != -1)
		{
			m_strNetworkLogFileName = std::format("{}\\GeneralsOnlineData\\GeneralsOnline_UserID_{}.log", TheGlobalData->getPath_UserData().str(), pAuthInterface->GetUserID());
		}
		else
		{
			return;
		}
#else
		m_strNetworkLogFileName = std::format("{}\\GeneralsOnlineData\\GeneralsOnline.log", TheGlobalData->getPath_UserData().str());
#endif
		/*
			std::stringstream ss;

#if defined(_DEBUG)
		if (IsDebuggerPresent())
		{
			ss << std::put_time(std::localtime(&in_time_t),
				"GeneralsOnline_Debugger_%Y-%m-%d-%H-%M-%S.log");
		}
		else
		{
			ss << std::put_time(std::localtime(&in_time_t),
				"GeneralsOnline_%Y-%m-%d-%H-%M-%S.log");
		}
#else
		ss << "GeneralsOnline.log";
#endif
*/
		std::ofstream overwriteFile(m_strNetworkLogFileName);

		// log start msg
		overwriteFile << std::put_time(std::localtime(&in_time_t), "Log Started at %Y/%m/%d %H:%M") << std::endl;
	}

	auto const rawNow = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	struct tm localNow = {};
	localtime_s(&localNow, &rawNow);
	char timebuf[32];
	strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", &localNow);

	char buffer[8192];
	va_list args;
	va_start(args, fmt);
	vsnprintf(buffer, 8192, fmt, args);
	buffer[8192 - 1] = 0;
	va_end(args);

	std::string strLogBuffer = std::format("[{}] {}", timebuf, buffer);

	// TODO_NGMP: Keep open and flush regularly
	std::ofstream logFile;
	logFile.open(m_strNetworkLogFileName, std::ios_base::app);
	logFile << strLogBuffer.c_str() << std::endl;
	logFile.close();

#if defined(GENERALS_ONLINE_BRANCH_JMARSHALL)
	DevConsole.AddLog(strLogBuffer.c_str());
#endif

	OutputDebugString(strLogBuffer.c_str());
	OutputDebugString("\n");
}

std::string Base64Encode(const std::vector<uint8_t>& data)
{
	static const char base64_chars[] =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"abcdefghijklmnopqrstuvwxyz"
		"0123456789+/";

	std::string encoded;
	size_t i = 0;
	uint32_t octet_a, octet_b, octet_c, triple;

	while (i < data.size())
	{
		octet_a = i < data.size() ? data[i++] : 0;
		octet_b = i < data.size() ? data[i++] : 0;
		octet_c = i < data.size() ? data[i++] : 0;

		triple = (octet_a << 16) | (octet_b << 8) | octet_c;

		encoded += base64_chars[(triple >> 18) & 0x3F];
		encoded += base64_chars[(triple >> 12) & 0x3F];
		encoded += (i >= data.size() + 1) ? '=' : base64_chars[(triple >> 6) & 0x3F];
		encoded += (i >= data.size())     ? '=' : base64_chars[triple & 0x3F];
	}

	return encoded;
}

std::vector<uint8_t> Base64Decode(const std::string& encodedData) {
	static const std::string base64Chars =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"abcdefghijklmnopqrstuvwxyz"
		"0123456789+/";

	auto isBase64 = [](unsigned char c) {
		return std::isalnum(c) || (c == '+') || (c == '/');
		};

	std::vector<uint8_t> decodedData;
	int inLen = encodedData.size();
	int i = 0;
	int in_ = 0;
	uint8_t charArray4[4], charArray3[3];

	while (inLen-- && (encodedData[in_] != '=') && isBase64(encodedData[in_])) {
		if (i >= 4) break;
		charArray4[i++] = encodedData[in_]; in_++;
		if (i == 4) {
			for (i = 0; i < 4; i++)
				charArray4[i] = base64Chars.find(charArray4[i]);

			charArray3[0] = (charArray4[0] << 2) + ((charArray4[1] & 0x30) >> 4);
			charArray3[1] = ((charArray4[1] & 0xf) << 4) + ((charArray4[2] & 0x3c) >> 2);
			charArray3[2] = ((charArray4[2] & 0x3) << 6) + charArray4[3];

			for (i = 0; i < 3; i++)
				decodedData.push_back(charArray3[i]);
			i = 0;
		}
	}

	if (i) {
		for (int j = i; j < 4; j++)
			charArray4[j] = 0;

		for (int j = 0; j < 4; j++)
			charArray4[j] = base64Chars.find(charArray4[j]);

		charArray3[0] = (charArray4[0] << 2) + ((charArray4[1] & 0x30) >> 4);
		charArray3[1] = ((charArray4[1] & 0xf) << 4) + ((charArray4[2] & 0x3c) >> 2);
		charArray3[2] = ((charArray4[2] & 0x3) << 6) + charArray4[3];

		for (int j = 0; j < i - 1; j++)
			decodedData.push_back(charArray3[j]);
	}

	return decodedData;
}

std::string getGameExeCRC()
{
#if !defined(_WIN32)
    return "";
#else
    HMODULE hModule = GetModuleHandle(NULL);
    if (!hModule) return "";

    PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)hModule;
    PIMAGE_NT_HEADERS ntHeader = (PIMAGE_NT_HEADERS)((BYTE*)hModule + dosHeader->e_lfanew);

    PIMAGE_SECTION_HEADER section = IMAGE_FIRST_SECTION(ntHeader);
    for (int i = 0; i < ntHeader->FileHeader.NumberOfSections; i++, section++) {
        if (strcmp((char*)section->Name, ".text") == 0) {
            BYTE* codeBase = (BYTE*)hModule + section->VirtualAddress;
            DWORD codeSize = section->Misc.VirtualSize;

            HCRYPTPROV hProv;
            HCRYPTHASH hHash;
            BYTE hash[32]; // SHA-256
            DWORD hashLen = sizeof(hash);

            if (CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT) &&
                CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash) &&
                CryptHashData(hHash, codeBase, codeSize, 0) &&
                CryptGetHashParam(hHash, HP_HASHVAL, hash, &hashLen, 0)) {

                std::ostringstream oss;
                oss << std::hex << std::setfill('0');
                for (DWORD j = 0; j < hashLen; j++) {
                    oss << std::setw(2) << static_cast<int>(hash[j]);
                }

                CryptDestroyHash(hHash);
                CryptReleaseContext(hProv, 0);

                return oss.str(); // return as std::string
            }

            CryptDestroyHash(hHash);
            CryptReleaseContext(hProv, 0);
        }
    }
    return "";
#endif
}

int RoundUpLatencyToFrameInterval(int latency, int frameInterval)
{
	if (frameInterval == 0)
		return latency;

	int remainder = latency % frameInterval;
	if (remainder == 0)
		return latency;

	return latency + frameInterval - remainder;
}
int ConvertMSLatencyToFrames(int ms)
{
	ms = RoundUpLatencyToFrameInterval(ms, 1000 / GENERALS_ONLINE_HIGH_FPS_LIMIT);
	return (int)ceil((ms / 1000.f) * (float)GENERALS_ONLINE_HIGH_FPS_LIMIT);
}

int ConvertMSLatencyToGenToolFrames(int ms)
{
	return (int)ceil((float)ConvertMSLatencyToFrames(ms) / (float)GENERALS_ONLINE_HIGH_FPS_FRAME_MULTIPLIER);
}

#if defined(__APPLE__)
std::string GetMachineGuid()
{
	std::string strUUID = NGMP_Apple_GetPlatformUUID();
	if (strUUID.empty())
	{
		return "";
	}
	return strUUID.append(std::string("YY"));
}

std::string GetPrimaryMacAddress()
{
	return NGMP_Apple_GetPrimaryMacAddress();
}

std::string GetVolumeSerial()
{
	return "";
}
#elif !defined(_WIN32)
std::string GetMachineGuid()
{
	return "";
}

std::string GetPrimaryMacAddress()
{
	return "";
}

std::string GetVolumeSerial()
{
	return "";
}
#else
#include <windows.h>
#include <iphlpapi.h>
#include <iostream>
#include <string>

#pragma comment(lib, "iphlpapi.lib")

// Helper: read MachineGuid from registry
std::string GetMachineGuid() {
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
        "SOFTWARE\\Microsoft\\Cryptography",
        0, KEY_READ | KEY_WOW64_64KEY, &hKey) != ERROR_SUCCESS) {
        return "";
    }

    char buffer[256];
    DWORD bufferSize = sizeof(buffer);
    DWORD type = 0;
    if (RegQueryValueExA(hKey, "MachineGuid", nullptr, &type,
        reinterpret_cast<LPBYTE>(buffer), &bufferSize) == ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return std::string(buffer, bufferSize - 1).append(std::string("YY")); // remove null terminator
    }

    RegCloseKey(hKey);
    return "";
}

// Helper: get MAC address of first adapter
std::string GetPrimaryMacAddress() {
    PIP_ADAPTER_INFO adapterInfo;
    DWORD bufLen = sizeof(IP_ADAPTER_INFO);
    adapterInfo = (IP_ADAPTER_INFO*)malloc(bufLen);

    if (GetAdaptersInfo(adapterInfo, &bufLen) == ERROR_BUFFER_OVERFLOW) {
        free(adapterInfo);
        adapterInfo = (IP_ADAPTER_INFO*)malloc(bufLen);
    }

    if (GetAdaptersInfo(adapterInfo, &bufLen) == NO_ERROR) {
        char macAddr[32];
        sprintf_s(macAddr, "%02X:%02X:%02X:%02X:%02X:%02XZZ",
            adapterInfo->Address[0], adapterInfo->Address[1],
            adapterInfo->Address[2], adapterInfo->Address[3],
            adapterInfo->Address[4], adapterInfo->Address[5]);
        free(adapterInfo);
        return std::string(macAddr);
    }

    free(adapterInfo);
    return "";
}

// Helper: get Volume Serial Number of C: drive
std::string GetVolumeSerial() {
    DWORD serialNumber = 0;
    if (GetVolumeInformationA("C:\\", nullptr, 0, &serialNumber,
        nullptr, nullptr, nullptr, 0)) {
        char serialStr[32];
        sprintf_s(serialStr, "%08XZZ", serialNumber);
        return std::string(serialStr);
    }
    return "";
}
#endif
