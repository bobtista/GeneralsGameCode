#pragma once

// Portability helpers for the Generals Online service layer on non-Windows platforms.
// Windows builds see nothing from this header.

#if !defined(_WIN32)

#include <chrono>
#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>

// libc++ does not provide std::chrono::utc_clock yet. GO only uses it for millisecond
// wall-clock timestamps, where utc_clock and system_clock differ only by leap seconds.
#if defined(_LIBCPP_VERSION) && !(defined(__cpp_lib_chrono) && __cpp_lib_chrono >= 201907L)
namespace std
{
	namespace chrono
	{
		using utc_clock = system_clock;
	}
}
#endif

#ifndef __debugbreak
#define __debugbreak() __builtin_debugtrap()
#endif

#ifndef _snprintf
#define _snprintf snprintf
#endif

#ifndef CP_UTF8
#define CP_UTF8 65001
#endif

template <size_t N>
inline int sprintf_s(char (&buffer)[N], const char* format, ...)
{
	va_list args;
	va_start(args, format);
	int result = vsnprintf(buffer, N, format, args);
	va_end(args);
	return result;
}

inline int memcpy_s(void* dest, size_t destSize, const void* src, size_t count)
{
	if (count == 0)
		return 0;
	if (dest == nullptr)
		return EINVAL;
	if (src == nullptr || destSize < count)
	{
		memset(dest, 0, destSize);
		return src == nullptr ? EINVAL : ERANGE;
	}
	memcpy(dest, src, count);
	return 0;
}

inline int localtime_s(struct tm* outTm, const time_t* inTime)
{
	return localtime_r(inTime, outTm) != nullptr ? 0 : -1;
}

// Win32 compat shim (win32_shims/windows.h)
#include <windows.h>

inline HMODULE LoadLibraryA(const char* path)
{
	return LoadLibrary(path);
}

// The process code page is UTF-8 on macOS and Linux.
inline UINT GetACP()
{
	return CP_UTF8;
}

inline DWORD GetCurrentDirectoryA(DWORD bufferLength, char* buffer)
{
	return GetCurrentDirectory(bufferLength, buffer);
}

inline BOOL SetEnvironmentVariableA(const char* name, const char* value)
{
	if (value == nullptr)
		return ::unsetenv(name) == 0 ? TRUE : FALSE;
	return ::setenv(name, value, 1) == 0 ? TRUE : FALSE;
}

#ifndef MOVEFILE_REPLACE_EXISTING
#define MOVEFILE_REPLACE_EXISTING 0x00000001
#endif
#ifndef MOVEFILE_WRITE_THROUGH
#define MOVEFILE_WRITE_THROUGH 0x00000008
#endif

// No DPAPI here. Encryption fails, so refresh tokens are never written to disk in plain text.
#ifndef _WINCRYPT_DATA_BLOB_DEFINED
#define _WINCRYPT_DATA_BLOB_DEFINED
typedef struct _CRYPTOAPI_BLOB
{
	DWORD cbData;
	BYTE* pbData;
} DATA_BLOB;
#endif

inline BOOL CryptProtectData(DATA_BLOB*, const wchar_t*, DATA_BLOB*, void*, void*, DWORD, DATA_BLOB* outBlob)
{
	if (outBlob != nullptr)
	{
		outBlob->cbData = 0;
		outBlob->pbData = nullptr;
	}
	return FALSE;
}

inline BOOL CryptUnprotectData(DATA_BLOB*, wchar_t**, DATA_BLOB*, void*, void*, DWORD, DATA_BLOB* outBlob)
{
	if (outBlob != nullptr)
	{
		outBlob->cbData = 0;
		outBlob->pbData = nullptr;
	}
	return FALSE;
}

// rename() replaces an existing target atomically on POSIX.
inline BOOL MoveFileExA(const char* existingFileName, const char* newFileName, DWORD)
{
	return ::rename(existingFileName, newFileName) == 0 ? TRUE : FALSE;
}

#endif
