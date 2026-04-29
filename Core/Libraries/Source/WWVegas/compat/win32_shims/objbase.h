#pragma once

#include "windows.h"

#include <string.h>

#ifndef GUID_DEFINED
#define GUID_DEFINED
typedef struct GUID {
    unsigned long Data1;
    unsigned short Data2;
    unsigned short Data3;
    unsigned char Data4[8];
} GUID;
#endif

#ifndef REFGUID
#ifdef __cplusplus
typedef const GUID &REFGUID;
typedef const GUID &REFIID;
typedef const GUID &REFCLSID;
#else
typedef const GUID *REFGUID;
typedef const GUID *REFIID;
typedef const GUID *REFCLSID;
#endif
#endif

typedef GUID IID;
typedef GUID CLSID;
typedef GUID *LPGUID;

#ifndef EXTERN_C
#ifdef __cplusplus
#define EXTERN_C extern "C"
#else
#define EXTERN_C extern
#endif
#endif

#ifndef STDAPI
#define STDAPI EXTERN_C HRESULT STDMETHODCALLTYPE
#endif

#ifndef STDAPI_
#define STDAPI_(type) EXTERN_C type STDMETHODCALLTYPE
#endif

#ifndef DEFINE_GUID
#define DEFINE_GUID(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
    static const GUID name = { l, w1, w2, { b1, b2, b3, b4, b5, b6, b7, b8 } }
#endif

// IID_IUnknown — the universal COM interface ID. Defined here so non-Windows
// builds that talk to COM stubs (StubD3D8Device, etc.) can compare against it.
#ifndef IID_IUnknown_DEFINED
#define IID_IUnknown_DEFINED
DEFINE_GUID(IID_IUnknown, 0x00000000, 0x0000, 0x0000, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46);
#endif

// GUID equality. Windows provides this via guiddef.h; our shim has to.
#ifndef _GUID_OPERATORS_DEFINED
#define _GUID_OPERATORS_DEFINED
#include <string.h>
inline bool operator==(const GUID &a, const GUID &b)
{
    return memcmp(&a, &b, sizeof(GUID)) == 0;
}
inline bool operator!=(const GUID &a, const GUID &b)
{
    return !(a == b);
}
#endif

#ifndef interface
#define interface struct
#endif

#ifndef DECLSPEC_NOVTABLE
#define DECLSPEC_NOVTABLE
#endif

#ifndef PURE
#define PURE = 0
#endif

#ifndef THIS_
#define THIS_
#endif

#ifndef THIS
#define THIS void
#endif

#ifndef STDMETHODCALLTYPE
#define STDMETHODCALLTYPE WINAPI
#endif

#ifndef STDMETHOD
#define STDMETHOD(method) virtual HRESULT STDMETHODCALLTYPE method
#endif

#ifndef STDMETHOD_
#define STDMETHOD_(type, method) virtual type STDMETHODCALLTYPE method
#endif

#ifndef DECLARE_INTERFACE
#define DECLARE_INTERFACE(iface) interface DECLSPEC_NOVTABLE iface
#endif

#ifndef DECLARE_INTERFACE_
#define DECLARE_INTERFACE_(iface, baseiface) interface DECLSPEC_NOVTABLE iface : public baseiface
#endif

#ifndef __IUnknown_INTERFACE_DEFINED__
#define __IUnknown_INTERFACE_DEFINED__
DECLARE_INTERFACE(IUnknown)
{
    STDMETHOD(QueryInterface)(THIS_ REFIID riid, void **ppvObject) PURE;
    STDMETHOD_(ULONG, AddRef)(THIS) PURE;
    STDMETHOD_(ULONG, Release)(THIS) PURE;
};
#endif

#ifndef _IUNKNOWN_DEFINED
#define _IUNKNOWN_DEFINED
#endif

#ifndef __IStream_FWD_DEFINED__
#define __IStream_FWD_DEFINED__
interface IStream;
#endif

#ifndef IsEqualGUID
inline int IsEqualGUID(const GUID &a, const GUID &b)
{
    return memcmp(&a, &b, sizeof(GUID)) == 0;
}
#endif
