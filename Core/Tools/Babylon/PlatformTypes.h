/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// PlatformTypes.h - Cross-platform type definitions
// TheSuperHackers @refactor bobtista 01/01/2025 Replace Windows/MFC types with cross-platform equivalents

#pragma once

#include <cstddef>
#include <climits>
#include <cwchar>

// OLECHAR is wchar_t on Windows (OLE character type)
// On other platforms, we use standard wchar_t
#ifndef OLECHAR
    typedef wchar_t OLECHAR;
#endif

// Windows VOID type (void on all platforms)
#ifndef VOID
    #define VOID void
#endif

// Boolean values (Windows macros)
#ifndef TRUE
    #define TRUE 1
#endif
#ifndef FALSE
    #define FALSE 0
#endif

// Path length constants
#ifndef _MAX_PATH
    #ifdef _WIN32
        #define _MAX_PATH 260
    #else
        #include <limits.h>
        #ifdef PATH_MAX
            #define _MAX_PATH PATH_MAX
        #else
            #define _MAX_PATH 4096  // Linux default max path length
        #endif
    #endif
#endif

// Windows-specific types (only defined on Windows, stubbed elsewhere)
#ifdef _WIN32
    // On Windows, use actual Windows types
    // Prevent macro conflicts with Qt - Qt defines QT_VERSION when any Qt header is included
    // Also check for Q_MOC_RUN (MOC processing) and Qt library macros
    #if !defined(Q_MOC_RUN) && !defined(QT_VERSION) && !defined(QT_WIDGETS_LIB) && !defined(QT_GUI_LIB) && !defined(QT_CORE_LIB)
        // No Qt detected - safe to include windows.h
        // Prevent macro conflicts with Qt
        #ifndef NOMINMAX
        #define NOMINMAX
        #endif
        #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
        #endif
        // Ensure we use ANSI functions, not Unicode
        #ifdef UNICODE
        #undef UNICODE
        #endif
        #ifdef _UNICODE
        #undef _UNICODE
        #endif
        #include <windows.h>
        // OLECHAR is already defined by windows.h as wchar_t
        #undef OLECHAR
    #else
        // Qt is present or MOC is running - define Windows types manually to avoid conflicts
        typedef unsigned short WORD;
        typedef unsigned long DWORD;
        typedef unsigned int UINT;
        typedef long LONG;
        // HWND is already defined by Qt's qwindowdefs_win.h when Qt headers are included
        // Do NOT redefine it - Qt's definition takes precedence
        // Only define BOOL if it's not already defined
        #ifndef BOOL
        typedef int BOOL;
        #endif
        // CALLBACK needs to be __stdcall for Windows callbacks, not empty
        #ifndef CALLBACK
        #define CALLBACK __stdcall
        #endif
        // Define Windows string types that might be needed
        typedef char CHAR;
        typedef wchar_t WCHAR;
        typedef CHAR* LPCH;
        typedef const CHAR* LPCCH;
        typedef WCHAR* LPWCH;
        typedef const WCHAR* LPCWCH;
    #endif
    typedef wchar_t OLECHAR;  // Ensure it's wchar_t
#else
    // Stub types for non-Windows platforms
    typedef void* HWND;
    typedef int BOOL;
    typedef unsigned short WORD;
    typedef unsigned long DWORD;
    typedef unsigned int UINT;
    typedef long LONG;
    typedef unsigned long ULONG;
    typedef void* HINSTANCE;
    typedef void* HDC;
    typedef void* HICON;
    typedef void* HMENU;
    typedef void* HBRUSH;
    typedef void* HPEN;
    typedef void* HFONT;
    typedef long LPARAM;
    typedef unsigned int WPARAM;
    typedef long LRESULT;
    typedef void* HKEY;
    typedef void* HANDLE;
    
    // Windows callback types (stubs)
    typedef int (__stdcall *WNDENUMPROC)(HWND, LPARAM);
    
    // Windows message types (not used on non-Windows)
    #define WM_USER 0x0400
    #define WM_COMMAND 0x0111
    #define WM_CLOSE 0x0010
    #define WM_DESTROY 0x0002
    #define WM_PAINT 0x000F
    #define WM_SIZE 0x0005
    #define WM_MOVE 0x0003
    #define WM_SETFOCUS 0x0007
    #define WM_KILLFOCUS 0x0008
    #define WM_KEYDOWN 0x0100
    #define WM_KEYUP 0x0101
    #define WM_CHAR 0x0102
    #define WM_LBUTTONDOWN 0x0201
    #define WM_LBUTTONUP 0x0202
    #define WM_RBUTTONDOWN 0x0204
    #define WM_RBUTTONUP 0x0205
    #define WM_MOUSEMOVE 0x0200
    #define WM_MOUSEWHEEL 0x020A
    #define WM_DROPFILES 0x0233
    #define WM_TIMER 0x0113
    
    // Windows constants (stubs for compilation)
    #define IDOK 1
    #define IDCANCEL 2
    #define IDYES 6
    #define IDNO 7
    #define IDABORT 3
    #define IDRETRY 4
    #define IDIGNORE 5
    
        // Windows error codes (stubs)
        #define ERROR_SUCCESS 0
        #define ERROR_FILE_NOT_FOUND 2
        #define ERROR_PATH_NOT_FOUND 3
        #define ERROR_ACCESS_DENIED 5
        #define ERROR_INVALID_HANDLE 6
        #define ERROR_NOT_ENOUGH_MEMORY 8
        #define ERROR_INVALID_PARAMETER 87
        #define ERROR_INSUFFICIENT_BUFFER 122

        // Windows code page constants (stubs)
        #define CP_ACP 0  // ANSI code page
        #define MB_ERR_INVALID_CHARS 0x00000008  // Error on invalid characters

        // Windows string conversion functions (stubs)
        inline int MultiByteToWideChar(unsigned int CodePage, unsigned long dwFlags, const char* lpMultiByteStr, int cbMultiByte, wchar_t* lpWideCharStr, int cchWideChar) {
            // Simple stub: just copy bytes as-is (not a real implementation)
            if (cbMultiByte <= 0 || cchWideChar <= 0) return 0;
            int i;
            for (i = 0; i < cbMultiByte && i < cchWideChar; i++) {
                lpWideCharStr[i] = (wchar_t)(unsigned char)lpMultiByteStr[i];
            }
            if (i < cchWideChar) lpWideCharStr[i] = 0;
            return i;
        }
        inline DWORD GetLastError() { return 0; }  // Stub: always return success
    
    // Windows file attributes (stubs)
    #define FILE_ATTRIBUTE_DIRECTORY 0x00000010
    #define FILE_ATTRIBUTE_NORMAL 0x00000080
    #define FILE_ATTRIBUTE_READONLY 0x00000001
    
    // Windows file access (stubs)
    #define GENERIC_READ 0x80000000
    #define GENERIC_WRITE 0x40000000
    #define FILE_SHARE_READ 0x00000001
    #define FILE_SHARE_WRITE 0x00000002
    #define OPEN_EXISTING 3
    #define CREATE_ALWAYS 2
    #define FILE_ATTRIBUTE_NORMAL 0x00000080
    
    // Windows memory (stubs)
    #define MEM_COMMIT 0x1000
    #define MEM_RESERVE 0x2000
    #define MEM_RELEASE 0x8000
    #define PAGE_READWRITE 0x04
    
    // Windows registry (stubs - not used on non-Windows)
    #define HKEY_CURRENT_USER ((HKEY)(ULONG_PTR)((LONG)0x80000001))
    #define HKEY_LOCAL_MACHINE ((HKEY)(ULONG_PTR)((LONG)0x80000002))
    #define KEY_READ 0x20019
    #define KEY_WRITE 0x20006
    #define REG_SZ 1
    #define REG_DWORD 4
    
    // Windows OLE (stubs - not used on non-Windows)
    #define S_OK 0
    #define S_FALSE 1
    #define E_FAIL 0x80004005
    #define E_INVALIDARG 0x80070057
    #define E_OUTOFMEMORY 0x8007000E
    #define E_NOTIMPL 0x80004001
    #define E_NOINTERFACE 0x80004002
    #define E_POINTER 0x80004003
    #define E_UNEXPECTED 0x8000FFFF
    #define E_ABORT 0x80004004
    #define E_ACCESSDENIED 0x80070005
    #define CO_E_NOTINITIALIZED 0x800401F0
    #define CO_E_ALREADYINITIALIZED 0x800401F1
    
    // Windows calling conventions (stubs)
    #define __stdcall
    #define __cdecl
    #define __fastcall
    #define __thiscall
    #define CALLBACK
    #define WINAPI
    #define APIENTRY
    #define WINAPIV
    
    // Windows string types (stubs)
    typedef wchar_t* LPWSTR;
    typedef const wchar_t* LPCWSTR;
    typedef char* LPSTR;
    typedef const char* LPCSTR;
    typedef OLECHAR* LPOLESTR;
    typedef const OLECHAR* LPCOLESTR;
    
    // Windows GUID (stubs)
    typedef struct {
        unsigned long Data1;
        unsigned short Data2;
        unsigned short Data3;
        unsigned char Data4[8];
    } GUID;
    
    typedef GUID* REFGUID;
    typedef GUID* REFIID;
    typedef GUID* REFCLSID;
    
    // Windows VARIANT (stubs - for OLE)
    typedef struct tagVARIANT {
        unsigned short vt;
        unsigned short wReserved1;
        unsigned short wReserved2;
        unsigned short wReserved3;
        union {
            long lVal;
            double dblVal;
            void* pvVal;
            OLECHAR* bstrVal;
        };
    } VARIANT;
    typedef VARIANT* LPVARIANT;
    
    // Windows BSTR (stubs - for OLE)
    typedef OLECHAR* BSTR;
    
    // VARIANT access macros (stubs)
    #define V_VT(pvar) ((pvar)->vt)
    #define V_BSTR(pvar) ((pvar)->bstrVal)
    #define V_BOOL(pvar) ((pvar)->lVal)
    
    // VARIANT type constants (stubs)
    #define VT_EMPTY 0
    #define VT_NULL 1
    #define VT_I2 2
    #define VT_I4 3
    #define VT_R4 4
    #define VT_R8 5
    #define VT_CY 6
    #define VT_DATE 7
    #define VT_BSTR 8
    #define VT_DISPATCH 9
    #define VT_ERROR 10
    #define VT_BOOL 11
    #define VT_VARIANT 12
    #define VT_UNKNOWN 13
    
    // Windows IUnknown (stubs - for OLE)
    struct IUnknown {
        virtual long QueryInterface(const GUID& riid, void** ppvObject) = 0;
        virtual unsigned long AddRef() = 0;
        virtual unsigned long Release() = 0;
    };
    
    // Windows IDispatch (stubs - for OLE)
    struct IDispatch : public IUnknown {
        virtual long GetTypeInfoCount(unsigned int* pctinfo) = 0;
        virtual long GetTypeInfo(unsigned int iTInfo, unsigned long lcid, void** ppTInfo) = 0;
        virtual long GetIDsOfNames(const GUID& riid, OLECHAR** rgszNames, unsigned int cNames, unsigned long lcid, long* rgDispId) = 0;
        virtual long Invoke(long dispIdMember, const GUID& riid, unsigned long lcid, unsigned short wFlags, void* pDispParams, void* pVarResult, void* pExcepInfo, unsigned int* puArgErr) = 0;
    };
    
    // Windows COM dispatch types (stubs) - defined after IUnknown and IDispatch
    typedef IDispatch* LPDISPATCH;
    typedef IUnknown* LPUNKNOWN;
    typedef const char* LPCTSTR;
    
    // COM function stubs
    inline BSTR SysAllocString(const OLECHAR* psz) {
        if (!psz) return nullptr;
        size_t len = wcslen(psz);
        BSTR bstr = new OLECHAR[len + 1];
        wcscpy(bstr, psz);
        return bstr;
    }
    inline void SysFreeString(BSTR bstr) {
        if (bstr) delete[] bstr;
    }
    inline void VariantClear(VARIANT* pvar) {
        if (pvar->vt == VT_BSTR && pvar->bstrVal) {
            SysFreeString(pvar->bstrVal);
        }
        pvar->vt = VT_EMPTY;
    }
    
    // Windows functions (stubs - provide empty implementations or return errors)
    inline BOOL EnumWindows(WNDENUMPROC lpEnumFunc, LPARAM lParam) { return FALSE; }
    inline int GetWindowText(HWND hWnd, char* lpString, int nMaxCount) { return 0; }
    inline HWND SetForegroundWindow(HWND hWnd) { return nullptr; }
    inline BOOL AfxOleInit() { return FALSE; }  // OLE not available on non-Windows
    // _getcwd is Windows-specific, use getcwd on Unix
    // Note: This is a stub - actual implementation should use getcwd from unistd.h
    // The caller should include <unistd.h> and use getcwd directly
    inline char* strlwr(char* str) {
        // POSIX doesn't have strlwr, implement it
        for (char* p = str; *p; ++p) {
            *p = tolower(*p);
        }
        return str;
    }
#endif

// Standard C++ wide string functions (available on all platforms)
// These are used by OLEString and work cross-platform
// wcslen, wcscpy, wcscmp, swprintf, etc. are all standard C++

// Cross-platform wcsicmp (case-insensitive wide string compare)
// Windows has wcsicmp, Unix has wcscasecmp
#ifndef _WIN32
    #include <wchar.h>
    #include <strings.h>  // For strcasecmp
    #include <cctype>     // For toupper
    #ifndef wcsicmp
        #define wcsicmp wcscasecmp
    #endif
    #ifndef wcsnicmp
        #define wcsnicmp wcsncasecmp
    #endif
    #ifndef stricmp
        #define stricmp strcasecmp
    #endif
    // wcsupr doesn't exist on POSIX, implement it
    inline wchar_t* wcsupr(wchar_t* str) {
        if (str) {
            for (wchar_t* p = str; *p; ++p) {
                *p = towupper(*p);
            }
        }
        return str;
    }
    // wcslwr doesn't exist on POSIX, implement it
    inline wchar_t* wcslwr(wchar_t* str) {
        if (str) {
            for (wchar_t* p = str; *p; ++p) {
                *p = towlower(*p);
            }
        }
        return str;
    }
#endif

// MFC Tree Control stubs (for non-Windows)
#ifndef _WIN32
    // Forward declarations for MFC tree control types
    // These are used in TransDB.h but not implemented on non-Windows
    class CTreeCtrl;  // MFC tree control - not available on non-Windows
    typedef void* HTREEITEM;  // Tree item handle - use void* as stub
#endif

// Additional helper macros
#ifndef MAX_PATH
    #define MAX_PATH _MAX_PATH
#endif

// Ensure we have standard boolean support
#include <cstdbool>
#ifndef __cplusplus
    #include <stdbool.h>
#endif

