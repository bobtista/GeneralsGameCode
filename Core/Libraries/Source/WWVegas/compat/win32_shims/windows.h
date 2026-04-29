#pragma once

#include "bittype.h"

#include <cmath>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef CALLBACK
#define CALLBACK
#endif

#ifndef WINAPI
#define WINAPI
#endif

#ifndef __stdcall
#define __stdcall
#endif

#ifndef __cdecl
#define __cdecl
#endif

#ifndef __fastcall
#define __fastcall
#endif

// MSVC string-comparison spellings. POSIX has the same APIs without the
// underscore.
#include <strings.h>
#ifndef _stricmp
#define _stricmp strcasecmp
#endif
#ifndef _strnicmp
#define _strnicmp strncasecmp
#endif
#ifndef stricmp
#define stricmp strcasecmp
#endif
#ifndef strnicmp
#define strnicmp strncasecmp
#endif
#ifndef _wcsicmp
#define _wcsicmp wcscasecmp
#endif

// MSVC float-control register API (no-op on non-Win; the engine uses these to
// pin x87 precision, which doesn't apply on macOS/Linux x86_64/arm64 builds
// since SSE/scalar math has fixed precision).
#ifndef _MCW_RC
#define _MCW_RC   0x00000300
#endif
#ifndef _RC_NEAR
#define _RC_NEAR  0x00000000
#endif
#ifndef _RC_DOWN
#define _RC_DOWN  0x00000100
#endif
#ifndef _RC_UP
#define _RC_UP    0x00000200
#endif
#ifndef _RC_CHOP
#define _RC_CHOP  0x00000300
#endif
#ifndef _MCW_PC
#define _MCW_PC   0x00030000
#endif
#ifndef _PC_24
#define _PC_24    0x00020000
#endif
#ifndef _PC_53
#define _PC_53    0x00010000
#endif
#ifndef _PC_64
#define _PC_64    0x00000000
#endif
#ifndef _MCW_EM
#define _MCW_EM   0x0008001f
#endif

inline void _fpreset() {}
inline unsigned int _controlfp(unsigned int /*newctrl*/, unsigned int /*mask*/) { return 0; }
inline unsigned int _statusfp() { return 0; }
inline void _clearfp() {}

// _wtoi is the wide-char companion to atoi; provide a wcstol-based shim.
#include <cwchar>
#include <cwctype>
inline int _wtoi(const wchar_t *str)
{
    if (str == nullptr)
    {
        return 0;
    }
    return static_cast<int>(::wcstol(str, nullptr, 10));
}

// itoa is non-standard; some Win headers expose it. Provide a snprintf-backed
// version so legacy callers keep working.
#include <cstdio>
inline char *itoa(int value, char *buffer, int radix)
{
    if (buffer == nullptr)
    {
        return nullptr;
    }
    if (radix == 10)
    {
        std::snprintf(buffer, 16, "%d", value);
    }
    else if (radix == 16)
    {
        std::snprintf(buffer, 16, "%x", value);
    }
    else if (radix == 8)
    {
        std::snprintf(buffer, 16, "%o", value);
    }
    else
    {
        std::snprintf(buffer, 16, "%d", value);
    }
    return buffer;
}

// MSVC integer-size keywords. Westwood code spells 64-bit integers as
// `__int64` / `unsigned __int64`. Map to long long on non-Windows.
#ifndef __int64
#define __int64 long long
#endif

#ifndef _int64
#define _int64 long long
#endif

#ifndef __forceinline
#define __forceinline inline __attribute__((always_inline))
#endif

#ifndef DECLARE_HANDLE
#define DECLARE_HANDLE(name) typedef void *name
#endif

typedef void *HANDLE;
typedef HANDLE HWND;
typedef HANDLE HINSTANCE;
typedef HANDLE HDC;
typedef HANDLE HGDIOBJ;
typedef HANDLE HBITMAP;
typedef HANDLE HFONT;
typedef HANDLE HKEY;
typedef HANDLE HMODULE;
// FARPROC must be a function pointer type so consumers can call through it.
typedef int (*FARPROC)();

#ifndef HMONITOR_DECLARED
#define HMONITOR_DECLARED
DECLARE_HANDLE(HMONITOR);
#endif

typedef long LONG;
typedef int INT;
typedef float FLOAT;
typedef long HRESULT;
typedef void VOID;
typedef void *LPVOID;
typedef void *PVOID;
typedef const void *LPCVOID;
typedef const char *LPCSTR;
typedef char *LPSTR;
typedef BYTE *PBYTE;
typedef BYTE *LPBYTE;
typedef DWORD *LPDWORD;
typedef wchar_t *LPWSTR;
typedef const wchar_t *LPCWSTR;
typedef size_t SIZE_T;
typedef uintptr_t UINT_PTR;
typedef uintptr_t ULONG_PTR;
typedef uintptr_t DWORD_PTR;
typedef intptr_t INT_PTR;
typedef intptr_t LONG_PTR;

typedef struct _SYSTEMTIME {
    WORD wYear;
    WORD wMonth;
    WORD wDayOfWeek;
    WORD wDay;
    WORD wHour;
    WORD wMinute;
    WORD wSecond;
    WORD wMilliseconds;
} SYSTEMTIME;

typedef struct _FILETIME {
    DWORD dwLowDateTime;
    DWORD dwHighDateTime;
} FILETIME;

typedef union _LARGE_INTEGER {
    struct {
        DWORD LowPart;
        LONG HighPart;
    };
    long long QuadPart;
} LARGE_INTEGER;

typedef LONG *LPLONG;
typedef char CHAR;
typedef CHAR *LPCH;
typedef CHAR *PSTR;
// Win message-handler parameter types.
typedef uintptr_t WPARAM;
typedef intptr_t  LPARAM;
typedef intptr_t  LRESULT;
// PBITMAPINFO / PBITMAPINFOHEADER — pointer aliases the Win SDK adds.
struct tagBITMAPINFO;
struct tagBITMAPINFOHEADER;
typedef struct tagBITMAPINFO *PBITMAPINFO;
typedef struct tagBITMAPINFO *LPBITMAPINFO;
typedef struct tagBITMAPINFOHEADER *PBITMAPINFOHEADER;
typedef struct tagBITMAPINFOHEADER *LPBITMAPINFOHEADER;
inline int IsIconic(HWND) { return 0; }
typedef HANDLE HCURSOR;
inline HCURSOR SetCursor(HCURSOR) { return nullptr; }
typedef struct tagPOINT POINT;
inline int GetCursorPos(POINT *) { return 1; }
inline int ScreenToClient(HWND, POINT *) { return 1; }
inline int ClientToScreen(HWND, POINT *) { return 1; }
inline HANDLE GetProcessHeap() { return nullptr; }
inline void *HeapAlloc(HANDLE, DWORD, size_t bytes) { return std::calloc(1, bytes); }
inline int HeapFree(HANDLE, DWORD, void *p) { std::free(p); return 1; }
#ifndef HEAP_ZERO_MEMORY
#define HEAP_ZERO_MEMORY 0x00000008
#endif
typedef void *HLOCAL;
inline void *LocalAlloc(unsigned int /*flags*/, size_t bytes) { return std::calloc(1, bytes); }
inline void *LocalFree(void *p) { std::free(p); return nullptr; }
#ifndef LPTR
#define LPTR 0x40
#endif

// Win file API constants/stubs for screenshot helper paths.
#ifndef GENERIC_READ
#define GENERIC_READ  0x80000000
#endif
#ifndef GENERIC_WRITE
#define GENERIC_WRITE 0x40000000
#endif
#ifndef CREATE_ALWAYS
#define CREATE_ALWAYS 2
#endif
#ifndef OPEN_EXISTING
#define OPEN_EXISTING 3
#endif
#ifndef FILE_ATTRIBUTE_NORMAL
#define FILE_ATTRIBUTE_NORMAL 0x80
#endif
inline HANDLE CreateFile(const char *, DWORD, DWORD, void *, DWORD, DWORD, void *) { return nullptr; }
inline HANDLE CreateFileA(const char *, DWORD, DWORD, void *, DWORD, DWORD, void *) { return nullptr; }
inline int WriteFile(HANDLE, const void *, DWORD, DWORD *, void *) { return 0; }
inline int ReadFile(HANDLE, void *, DWORD, DWORD *, void *) { return 0; }

// IDispatch stub for COM calls in W3DWebBrowser.cpp (Win-only path).
struct IDispatch;
typedef IDispatch *LPDISPATCH;
#ifndef OPTIONAL
#define OPTIONAL
#endif

inline unsigned int GetDoubleClickTime() { return 500; }
inline const char *GetCommandLineA() { return ""; }
inline DWORD GetModuleFileName(HMODULE, char *, DWORD size) { (void)size; return 0; }
inline DWORD GetModuleFileNameA(HMODULE, char *, DWORD size) { (void)size; return 0; }
inline void GetLocalTime(SYSTEMTIME *t) { if (t) { *t = SYSTEMTIME{}; } }
inline DWORD GetLastError() { return static_cast<DWORD>(errno); }
inline void SetLastError(DWORD code) { errno = static_cast<int>(code); }

#include <time.h>
inline int QueryPerformanceFrequency(LARGE_INTEGER *freq)
{
    if (freq == nullptr) { return 0; }
    freq->QuadPart = 1000000000LL;
    return 1;
}
inline int QueryPerformanceCounter(LARGE_INTEGER *cnt)
{
    if (cnt == nullptr) { return 0; }
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    cnt->QuadPart = static_cast<long long>(ts.tv_sec) * 1000000000LL + ts.tv_nsec;
    return 1;
}

// Win file enumeration API. Adapted from fbraz3/GeneralsX file_compat.h.
#include "file_compat.h"

// Include the winsock shim for everyone — many engine TUs use socket types
// without explicitly including <winsock.h>, relying on <windows.h> to pull
// it in transitively (which it does with WIN32_LEAN_AND_MEAN cleared).
#include "winsock.h"

// OSVERSIONINFO + GetVersionEx stubs. Adapted from GeneralsX windows_compat.h.
typedef struct _OSVERSIONINFO {
    DWORD dwOSVersionInfoSize;
    DWORD dwMajorVersion;
    DWORD dwMinorVersion;
    DWORD dwBuildNumber;
    DWORD dwPlatformId;
    char szCSDVersion[128];
} OSVERSIONINFO;

inline int GetVersionEx(OSVERSIONINFO * /*info*/) { return 0; }

typedef struct _MEMORYSTATUS {
    DWORD dwLength;
    DWORD dwMemoryLoad;
    DWORD dwTotalPhys;
    DWORD dwAvailPhys;
    DWORD dwTotalPageFile;
    DWORD dwAvailPageFile;
    DWORD dwTotalVirtual;
    DWORD dwAvailVirtual;
} MEMORYSTATUS, *LPMEMORYSTATUS;

inline void GlobalMemoryStatus(MEMORYSTATUS *m)
{
    if (m == nullptr)
    {
        return;
    }
    *m = MEMORYSTATUS{};
    m->dwLength = sizeof(*m);
}

// Win virtual-key codes (subset that engine GUI uses).
#ifndef VK_BACK
#define VK_BACK     0x08
#endif
#ifndef VK_TAB
#define VK_TAB      0x09
#endif
#ifndef VK_RETURN
#define VK_RETURN   0x0D
#endif
#ifndef VK_SHIFT
#define VK_SHIFT    0x10
#endif
#ifndef VK_CONTROL
#define VK_CONTROL  0x11
#endif
#ifndef VK_ESCAPE
#define VK_ESCAPE   0x1B
#endif
#ifndef VK_SPACE
#define VK_SPACE    0x20
#endif
#ifndef VK_DELETE
#define VK_DELETE   0x2E
#endif
#ifndef VK_INSERT
#define VK_INSERT   0x2D
#endif
#ifndef VK_F1
#define VK_F1       0x70
#endif
#ifndef VK_F2
#define VK_F2       0x71
#endif
#ifndef VK_F3
#define VK_F3       0x72
#endif
#ifndef VK_F4
#define VK_F4       0x73
#endif
#ifndef VK_F5
#define VK_F5       0x74
#endif
#ifndef VK_F6
#define VK_F6       0x75
#endif
#ifndef VK_F7
#define VK_F7       0x76
#endif
#ifndef VK_F8
#define VK_F8       0x77
#endif
#ifndef VK_F9
#define VK_F9       0x78
#endif
#ifndef VK_F10
#define VK_F10      0x79
#endif
inline short GetAsyncKeyState(int) { return 0; }

#ifndef LOCALE_SYSTEM_DEFAULT
#define LOCALE_SYSTEM_DEFAULT 0x0800
#endif
#ifndef LOCALE_USER_DEFAULT
#define LOCALE_USER_DEFAULT 0x0400
#endif
#ifndef DATE_SHORTDATE
#define DATE_SHORTDATE 0x0001
#endif
#ifndef DATE_LONGDATE
#define DATE_LONGDATE 0x0002
#endif
#ifndef TIME_NOSECONDS
#define TIME_NOSECONDS 0x0002
#endif
#ifndef TIME_FORCE24HOURFORMAT
#define TIME_FORCE24HOURFORMAT 0x0008
#endif
#ifndef TIME_NOTIMEMARKER
#define TIME_NOTIMEMARKER 0x0004
#endif
#ifndef VER_PLATFORM_WIN32_WINDOWS
#define VER_PLATFORM_WIN32_WINDOWS 1
#endif
#ifndef VER_PLATFORM_WIN32_NT
#define VER_PLATFORM_WIN32_NT 2
#endif

inline int GetTimeFormat(unsigned long, unsigned long, const SYSTEMTIME *, const char *, char *buf, int bufsize)
{
    if (buf != nullptr && bufsize > 0) { buf[0] = '\0'; }
    return 0;
}
inline int GetDateFormatW(unsigned long, unsigned long, const SYSTEMTIME *, const wchar_t *, wchar_t *buf, int bufsize)
{
    if (buf != nullptr && bufsize > 0) { buf[0] = L'\0'; }
    return 0;
}
inline int GetTimeFormatW(unsigned long, unsigned long, const SYSTEMTIME *, const wchar_t *, wchar_t *buf, int bufsize)
{
    if (buf != nullptr && bufsize > 0) { buf[0] = L'\0'; }
    return 0;
}

// Win critical section types backed by pthread_mutex.
#include <pthread.h>
typedef pthread_mutex_t CRITICAL_SECTION;
inline void InitializeCriticalSection(CRITICAL_SECTION *cs)
{
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(cs, &attr);
    pthread_mutexattr_destroy(&attr);
}
inline void DeleteCriticalSection(CRITICAL_SECTION *cs) { pthread_mutex_destroy(cs); }
inline void EnterCriticalSection(CRITICAL_SECTION *cs) { pthread_mutex_lock(cs); }
inline void LeaveCriticalSection(CRITICAL_SECTION *cs) { pthread_mutex_unlock(cs); }
inline int CopyFileA(const char *, const char *, int) { return 0; }
inline int SetWindowText(HWND, const char *) { return 0; }
inline int SetWindowTextW(HWND, const wchar_t *) { return 0; }
inline int SetWindowTextA(HWND, const char *) { return 0; }
typedef HANDLE HKL;
inline HKL GetKeyboardLayout(DWORD) { return nullptr; }

// Win threading. Stubbed; pthread is the real backend on POSIX, but the
// engine's thread-spawning paths are limited (network lobby pings, etc.).
typedef DWORD (*LPTHREAD_START_ROUTINE)(void *);
inline HANDLE CreateThread(void *, DWORD, LPTHREAD_START_ROUTINE, void *, DWORD, DWORD *) { return nullptr; }
inline int TerminateThread(HANDLE, DWORD) { return 0; }
inline int WaitForSingleObject(HANDLE, DWORD) { return 0; }
#ifndef INFINITE
#define INFINITE 0xFFFFFFFFu
#endif
#ifndef WAIT_OBJECT_0
#define WAIT_OBJECT_0 0
#endif
#ifndef WAIT_TIMEOUT
#define WAIT_TIMEOUT 258
#endif

// GlobalAlloc / GlobalFree — Win heap APIs, mapped to malloc/free.
#ifndef GMEM_FIXED
#define GMEM_FIXED 0
#endif
#ifndef GMEM_ZEROINIT
#define GMEM_ZEROINIT 0x40
#endif
inline void *GlobalAlloc(unsigned int flags, size_t bytes)
{
    void *p = std::malloc(bytes);
    if (p && (flags & GMEM_ZEROINIT))
    {
        std::memset(p, 0, bytes);
    }
    return p;
}
inline void *GlobalFree(void *p) { std::free(p); return nullptr; }
inline size_t GlobalSize(void * /*p*/) { return 0; }
inline void *GlobalReAlloc(void *p, size_t bytes, unsigned int /*flags*/) { return std::realloc(p, bytes); }

inline int AddFontResource(const char *) { return 0; }
inline int AddFontResourceA(const char *) { return 0; }
inline int RemoveFontResource(const char *) { return 0; }
inline int RemoveFontResourceA(const char *) { return 0; }
inline HANDLE CreateMutex(void *, int, const char *) { return nullptr; }
inline HANDLE CreateMutexW(void *, int, const wchar_t *) { return nullptr; }
inline HANDLE CreateMutexA(void *, int, const char *) { return nullptr; }
inline int CloseHandle(HANDLE) { return 0; }
#ifndef ERROR_ALREADY_EXISTS
#define ERROR_ALREADY_EXISTS 183
#endif
#ifndef ERROR_FILE_NOT_FOUND
#define ERROR_FILE_NOT_FOUND 2
#endif
#ifndef ERROR_PATH_NOT_FOUND
#define ERROR_PATH_NOT_FOUND 3
#endif

inline int MessageBox(HWND, const char *, const char *, unsigned long) { return 0; }
inline int MessageBoxA(HWND, const char *, const char *, unsigned long) { return 0; }
inline int MessageBoxW(HWND, const wchar_t *, const wchar_t *, unsigned long) { return 0; }
inline int ShowWindow(HWND, int) { return 0; }
inline DWORD GetModuleFileNameW(HMODULE, wchar_t *, DWORD size) { (void)size; return 0; }

#ifndef MB_OK
#define MB_OK              0x00000000
#endif
#ifndef MB_OKCANCEL
#define MB_OKCANCEL        0x00000001
#endif
#ifndef MB_YESNO
#define MB_YESNO           0x00000004
#endif
#ifndef MB_ABORTRETRYIGNORE
#define MB_ABORTRETRYIGNORE 0x00000002
#endif
#ifndef MB_ICONERROR
#define MB_ICONERROR       0x00000010
#endif
#ifndef MB_ICONSTOP
#define MB_ICONSTOP        MB_ICONERROR
#endif
#ifndef MB_ICONWARNING
#define MB_ICONWARNING     0x00000030
#endif
#ifndef MB_ICONINFORMATION
#define MB_ICONINFORMATION 0x00000040
#endif
#ifndef MB_SYSTEMMODAL
#define MB_SYSTEMMODAL     0x00001000
#endif
#ifndef MB_TASKMODAL
#define MB_TASKMODAL       0x00002000
#endif

#ifndef SW_HIDE
#define SW_HIDE            0
#endif
#ifndef SW_SHOWNORMAL
#define SW_SHOWNORMAL      1
#endif
#ifndef SW_SHOW
#define SW_SHOW            5
#endif
#ifndef SW_SHOWNA
#define SW_SHOWNA          8
#endif

typedef struct _EXCEPTION_RECORD EXCEPTION_RECORD;
typedef struct _CONTEXT CONTEXT;
typedef struct _EXCEPTION_POINTERS {
    EXCEPTION_RECORD *ExceptionRecord;
    CONTEXT          *ContextRecord;
} EXCEPTION_POINTERS, *LPEXCEPTION_POINTERS;

#ifndef __max
#define __max(a, b) ((a) > (b) ? (a) : (b))
#endif
#ifndef __min
#define __min(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef _stat
#define _stat stat
#endif
#ifndef _S_IFDIR
#define _S_IFDIR S_IFDIR
#endif

#ifndef CreateDirectory
inline int CreateDirectory(const char *p, void * /*attrs*/)
{
    return mkdir(p, 0755) == 0;
}
inline int CreateDirectoryA(const char *p, void * /*attrs*/)
{
    return mkdir(p, 0755) == 0;
}
#endif

typedef struct tagPOINT {
    LONG x;
    LONG y;
} POINT;

typedef struct tagSIZE {
    LONG cx;
    LONG cy;
} SIZE;

typedef struct tagPOINTFLOAT {
    FLOAT x;
    FLOAT y;
} POINTFLOAT;

typedef struct tagRECT {
    LONG left;
    LONG top;
    LONG right;
    LONG bottom;
} RECT;

typedef RECT *LPRECT;
typedef const RECT *LPCRECT;

typedef struct tagMONITORINFO {
    DWORD cbSize;
    RECT rcMonitor;
    RECT rcWork;
    DWORD dwFlags;
} MONITORINFO;

typedef struct tagPALETTEENTRY {
    BYTE peRed;
    BYTE peGreen;
    BYTE peBlue;
    BYTE peFlags;
} PALETTEENTRY;

typedef struct tagBITMAPFILEHEADER {
    WORD bfType;
    DWORD bfSize;
    WORD bfReserved1;
    WORD bfReserved2;
    DWORD bfOffBits;
} BITMAPFILEHEADER;

typedef struct tagBITMAPINFOHEADER {
    DWORD biSize;
    LONG biWidth;
    LONG biHeight;
    WORD biPlanes;
    WORD biBitCount;
    DWORD biCompression;
    DWORD biSizeImage;
    LONG biXPelsPerMeter;
    LONG biYPelsPerMeter;
    DWORD biClrUsed;
    DWORD biClrImportant;
} BITMAPINFOHEADER;

typedef struct tagRGBQUAD {
    BYTE rgbBlue;
    BYTE rgbGreen;
    BYTE rgbRed;
    BYTE rgbReserved;
} RGBQUAD;

typedef struct tagBITMAPINFO {
    BITMAPINFOHEADER bmiHeader;
    RGBQUAD bmiColors[1];
} BITMAPINFO;

typedef struct tagTEXTMETRIC {
    LONG tmHeight;
    LONG tmAscent;
    LONG tmDescent;
    LONG tmInternalLeading;
    LONG tmExternalLeading;
    LONG tmAveCharWidth;
    LONG tmMaxCharWidth;
    LONG tmWeight;
    LONG tmOverhang;
    LONG tmDigitizedAspectX;
    LONG tmDigitizedAspectY;
    wchar_t tmFirstChar;
    wchar_t tmLastChar;
    wchar_t tmDefaultChar;
    wchar_t tmBreakChar;
    BYTE tmItalic;
    BYTE tmUnderlined;
    BYTE tmStruckOut;
    BYTE tmPitchAndFamily;
    BYTE tmCharSet;
} TEXTMETRIC;

typedef struct tagLOGFONTA {
    LONG lfHeight;
    LONG lfWidth;
    LONG lfEscapement;
    LONG lfOrientation;
    LONG lfWeight;
    BYTE lfItalic;
    BYTE lfUnderline;
    BYTE lfStrikeOut;
    BYTE lfCharSet;
    BYTE lfOutPrecision;
    BYTE lfClipPrecision;
    BYTE lfQuality;
    BYTE lfPitchAndFamily;
    char lfFaceName[32];
} LOGFONTA;

typedef LOGFONTA LOGFONT;

typedef struct _GLYPHMETRICSFLOAT {
    FLOAT gmfBlackBoxX;
    FLOAT gmfBlackBoxY;
    POINTFLOAT gmfptGlyphOrigin;
    FLOAT gmfCellIncX;
    FLOAT gmfCellIncY;
} GLYPHMETRICSFLOAT;

typedef GLYPHMETRICSFLOAT *LPGLYPHMETRICSFLOAT;

typedef struct _RGNDATAHEADER {
    DWORD dwSize;
    DWORD iType;
    DWORD nCount;
    DWORD nRgnSize;
    RECT rcBound;
} RGNDATAHEADER;

typedef struct _RGNDATA {
    RGNDATAHEADER rdh;
    char Buffer[1];
} RGNDATA;

#ifndef GUID_DEFINED
#define GUID_DEFINED
typedef struct GUID {
    unsigned long Data1;
    unsigned short Data2;
    unsigned short Data3;
    unsigned char Data4[8];
} GUID;
#endif

#ifndef CONST
#define CONST const
#endif

#ifndef FAR
#define FAR
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef S_OK
#define S_OK ((HRESULT)0L)
#endif

#ifndef S_FALSE
#define S_FALSE ((HRESULT)1L)
#endif

#ifndef E_UNEXPECTED
#define E_UNEXPECTED ((HRESULT)0x8000FFFFL)
#endif

#ifndef E_NOTIMPL
#define E_NOTIMPL ((HRESULT)0x80004001L)
#endif

#ifndef E_NOINTERFACE
#define E_NOINTERFACE ((HRESULT)0x80004002L)
#endif

#ifndef E_POINTER
#define E_POINTER ((HRESULT)0x80004003L)
#endif

#ifndef E_ABORT
#define E_ABORT ((HRESULT)0x80004004L)
#endif

#ifndef E_FAIL
#define E_FAIL ((HRESULT)0x80004005L)
#endif

#ifndef E_ACCESSDENIED
#define E_ACCESSDENIED ((HRESULT)0x80070005L)
#endif

#ifndef E_HANDLE
#define E_HANDLE ((HRESULT)0x80070006L)
#endif

#ifndef E_OUTOFMEMORY
#define E_OUTOFMEMORY ((HRESULT)0x8007000EL)
#endif

#ifndef E_INVALIDARG
#define E_INVALIDARG ((HRESULT)0x80070057L)
#endif

#ifndef FAILED
#define FAILED(hr) (((HRESULT)(hr)) < 0)
#endif

#ifndef SUCCEEDED
#define SUCCEEDED(hr) (((HRESULT)(hr)) >= 0)
#endif

#ifndef MAKE_HRESULT
#define MAKE_HRESULT(sev, fac, code) \
    ((HRESULT)(((unsigned long)(sev) << 31) | ((unsigned long)(fac) << 16) | ((unsigned long)(code))))
#endif

#ifndef SEVERITY_SUCCESS
#define SEVERITY_SUCCESS 0
#endif

#ifndef SEVERITY_ERROR
#define SEVERITY_ERROR 1
#endif

#ifndef FACILITY_NULL
#define FACILITY_NULL 0
#endif

#ifndef FACILITY_RPC
#define FACILITY_RPC 1
#endif

#ifndef FACILITY_DISPATCH
#define FACILITY_DISPATCH 2
#endif

#ifndef FACILITY_STORAGE
#define FACILITY_STORAGE 3
#endif

#ifndef FACILITY_ITF
#define FACILITY_ITF 4
#endif

#ifndef FACILITY_WIN32
#define FACILITY_WIN32 7
#endif

#ifndef LOWORD
#define LOWORD(l) ((WORD)((DWORD_PTR)(l) & 0xFFFF))
#endif

#ifndef HIWORD
#define HIWORD(l) ((WORD)((DWORD_PTR)(l) >> 16))
#endif

#ifndef LOBYTE
#define LOBYTE(w) ((BYTE)((WORD)(w) & 0xFF))
#endif

#ifndef HIBYTE
#define HIBYTE(w) ((BYTE)((WORD)(w) >> 8))
#endif

#ifndef WINVER
#define WINVER 0x0600
#endif

#ifndef MAX_PATH
#define MAX_PATH 260
#endif

#ifndef GWL_STYLE
#define GWL_STYLE (-16)
#endif

#ifndef SWP_NOZORDER
#define SWP_NOZORDER 0x0004
#endif

#ifndef MONITOR_DEFAULTTOPRIMARY
#define MONITOR_DEFAULTTOPRIMARY 0x00000001
#endif

#ifndef HWND_TOPMOST
#define HWND_TOPMOST ((HWND)(intptr_t)-1)
#endif

#ifndef INVALID_FILE_ATTRIBUTES
#define INVALID_FILE_ATTRIBUTES 0xFFFFFFFFu
#endif

#ifndef BI_RGB
#define BI_RGB 0
#endif

#ifndef DIB_RGB_COLORS
#define DIB_RGB_COLORS 0
#endif

#ifndef ETO_OPAQUE
#define ETO_OPAQUE 0x0002
#endif

#ifndef FW_NORMAL
#define FW_NORMAL 400
#endif

#ifndef FW_BOLD
#define FW_BOLD 700
#endif

#ifndef DEFAULT_CHARSET
#define DEFAULT_CHARSET 1
#endif

#ifndef OUT_DEFAULT_PRECIS
#define OUT_DEFAULT_PRECIS 0
#endif

#ifndef CLIP_DEFAULT_PRECIS
#define CLIP_DEFAULT_PRECIS 0
#endif

#ifndef ANTIALIASED_QUALITY
#define ANTIALIASED_QUALITY 4
#endif

#ifndef VARIABLE_PITCH
#define VARIABLE_PITCH 2
#endif

#ifndef RGB
#define RGB(r, g, b) ((DWORD)(((BYTE)(r)) | ((WORD)((BYTE)(g)) << 8) | (((DWORD)(BYTE)(b)) << 16)))
#endif

static inline char *_strdup(const char *src)
{
    return ::strdup(src);
}

static inline char *lstrcat(char *dst, const char *src)
{
    return ::strcat(dst, src);
}

static inline char *lstrcpy(char *dst, const char *src)
{
    return ::strcpy(dst, src);
}

static inline char *lstrcpyn(char *dst, const char *src, int max_len)
{
    if (max_len <= 0) {
        return dst;
    }

    ::strncpy(dst, src, static_cast<size_t>(max_len) - 1);
    dst[max_len - 1] = '\0';
    return dst;
}

static inline int lstrlen(const char *src)
{
    return static_cast<int>(::strlen(src));
}

static inline int lstrcmpi(const char *lhs, const char *rhs)
{
    return ::strcasecmp(lhs, rhs);
}

static inline char *strupr(char *src)
{
    if (src == nullptr) {
        return nullptr;
    }

    for (char *cursor = src; *cursor != '\0'; ++cursor) {
        *cursor = static_cast<char>(::toupper(static_cast<unsigned char>(*cursor)));
    }

    return src;
}

// TheSuperHackers @build bobtista 29/04/2026 GetCurrentDirectory is provided
// by file_compat.h.
#if 0
static inline DWORD GetCurrentDirectory(DWORD buffer_len, char *buffer)
{
    if (buffer == nullptr || buffer_len == 0) {
        return 0;
    }

    if (::getcwd(buffer, static_cast<size_t>(buffer_len)) == nullptr) {
        buffer[0] = '\0';
        return 0;
    }

    return static_cast<DWORD>(::strlen(buffer));
}
#endif

// TheSuperHackers @build bobtista 29/04/2026 GetFileAttributes is provided
// by file_compat.h (included earlier in this header).

static inline HMODULE LoadLibrary(const char *path)
{
    return ::dlopen(path, RTLD_NOW | RTLD_LOCAL);
}

static inline FARPROC GetProcAddress(HMODULE module, const char *name)
{
    return reinterpret_cast<FARPROC>(::dlsym(module, name));
}

static inline int FreeLibrary(HMODULE module)
{
    return (module != nullptr && ::dlclose(module) == 0) ? TRUE : FALSE;
}

static inline BOOL GetClientRect(HWND, LPRECT rect)
{
    if (rect == nullptr) {
        return FALSE;
    }

    rect->left = 0;
    rect->top = 0;
    rect->right = 0;
    rect->bottom = 0;
    return TRUE;
}

static inline LONG GetWindowLong(HWND, int)
{
    return 0;
}

static inline BOOL AdjustWindowRect(LPRECT, DWORD, BOOL)
{
    return TRUE;
}

static inline BOOL SetWindowPos(HWND, HWND, int, int, int, int, unsigned int)
{
    return TRUE;
}

static inline HMONITOR MonitorFromWindow(HWND, DWORD)
{
    return nullptr;
}

static inline BOOL GetMonitorInfo(HMONITOR, MONITORINFO *info)
{
    if (info == nullptr) {
        return FALSE;
    }

    info->rcMonitor.left = 0;
    info->rcMonitor.top = 0;
    info->rcMonitor.right = 1920;
    info->rcMonitor.bottom = 1080;
    info->rcWork = info->rcMonitor;
    return TRUE;
}

static inline void ZeroMemory(void *ptr, size_t size)
{
    ::memset(ptr, 0, size);
}

static inline HWND GetDesktopWindow()
{
    return nullptr;
}

static inline HDC GetDC(HWND)
{
    return nullptr;
}

static inline int ReleaseDC(HWND, HDC)
{
    return 1;
}

static inline BOOL SetDeviceGammaRamp(HDC, LPCVOID)
{
    return TRUE;
}

static inline BOOL ExtTextOutW(HDC, int, int, unsigned int, const RECT *, const wchar_t *, unsigned int, const int *)
{
    return TRUE;
}

static inline BOOL GetTextExtentPoint32W(HDC, const wchar_t *, int len, SIZE *size)
{
    if (size == nullptr) {
        return FALSE;
    }

    size->cx = len;
    size->cy = 1;
    return TRUE;
}

static inline int MulDiv(int number, int numerator, int denominator)
{
    return (denominator != 0) ? static_cast<int>((static_cast<long long>(number) * numerator) / denominator) : 0;
}

static inline HFONT CreateFont(int, int, int, int, int, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, const char *)
{
    return reinterpret_cast<HFONT>(1);
}

static inline HBITMAP CreateDIBSection(HDC, const BITMAPINFO *, unsigned int, void **bits, HANDLE, DWORD)
{
    if (bits != nullptr) {
        *bits = nullptr;
    }
    return reinterpret_cast<HBITMAP>(1);
}

static inline HDC CreateCompatibleDC(HDC)
{
    return reinterpret_cast<HDC>(1);
}

static inline HGDIOBJ SelectObject(HDC, HGDIOBJ object)
{
    return object;
}

static inline DWORD SetBkColor(HDC, DWORD color)
{
    return color;
}

static inline DWORD SetTextColor(HDC, DWORD color)
{
    return color;
}

static inline BOOL GetTextMetrics(HDC, TEXTMETRIC *metric)
{
    if (metric == nullptr) {
        return FALSE;
    }

    ZeroMemory(metric, sizeof(TEXTMETRIC));
    metric->tmHeight = 1;
    metric->tmAscent = 1;
    metric->tmAveCharWidth = 1;
    return TRUE;
}

static inline BOOL DeleteObject(HGDIOBJ)
{
    return TRUE;
}

static inline BOOL DeleteDC(HDC)
{
    return TRUE;
}

static inline int _isnan(double value)
{
    return std::isnan(value) ? 1 : 0;
}

static inline int _finite(double value)
{
    return std::isfinite(value) ? 1 : 0;
}
