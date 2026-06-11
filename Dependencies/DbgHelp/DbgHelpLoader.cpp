/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 TheSuperHackers
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

#include "DbgHelpLoader.h"

#include "Allocator/SystemAllocator.h"
#include "Utility/lazy_static.h"
#include "Utility/STLUtils.h"
#include "Utility/stringex.h"
#include <new>
#include <set>


namespace
{

typedef BOOL (WINAPI *SymInitialize_t)(HANDLE hProcess, PCSTR UserSearchPath, BOOL fInvadeProcess);
typedef BOOL (WINAPI *SymCleanup_t)(HANDLE hProcess);
typedef DWORD (WINAPI *SymLoadModule_t)(HANDLE hProcess, HANDLE hFile, PCSTR ImageName,
	PCSTR ModuleName, DWORD BaseOfDll, DWORD SizeOfDll);
typedef DWORD (WINAPI *SymGetModuleBase_t)(HANDLE hProcess, DWORD dwAddr);
typedef BOOL (WINAPI *SymUnloadModule_t)(HANDLE hProcess, DWORD BaseOfDll);
typedef BOOL (WINAPI *SymGetSymFromAddr_t)(HANDLE hProcess, DWORD dwAddr, PDWORD pdwDisplacement, PIMAGEHLP_SYMBOL Symbol);
typedef BOOL (WINAPI *SymGetLineFromAddr_t)(HANDLE hProcess, DWORD dwAddr, PDWORD pdwDisplacement, PIMAGEHLP_LINE Line);
typedef DWORD (WINAPI *SymSetOptions_t)(DWORD SymOptions);
typedef PVOID (WINAPI *SymFunctionTableAccess_t)(HANDLE hProcess, DWORD AddrBase);
typedef BOOL (WINAPI *StackWalk_t)(DWORD MachineType, HANDLE hProcess, HANDLE hThread, LPSTACKFRAME StackFrame,
	PVOID ContextRecord, PREAD_PROCESS_MEMORY_ROUTINE ReadMemoryRoutine,
	PFUNCTION_TABLE_ACCESS_ROUTINE FunctionTableAccessRoutine, PGET_MODULE_BASE_ROUTINE GetModuleBaseRoutine,
	PTRANSLATE_ADDRESS_ROUTINE TranslateAddress);
typedef DWORD64 (WINAPI *SymGetModuleBase64_t)(HANDLE hProcess, DWORD64 dwAddr);
typedef DWORD64 (WINAPI *SymLoadModule64_t)(HANDLE hProcess, HANDLE hFile, PCSTR ImageName,
	PCSTR ModuleName, DWORD64 BaseOfDll, DWORD SizeOfDll);
typedef BOOL (WINAPI *SymGetSymFromAddr64_t)(HANDLE hProcess, DWORD64 Address, PDWORD64 Displacement, PIMAGEHLP_SYMBOL64 Symbol);
typedef BOOL (WINAPI *SymGetLineFromAddr64_t)(HANDLE hProcess, DWORD64 dwAddr, PDWORD pdwDisplacement, PIMAGEHLP_LINE64 Line);
typedef PVOID (WINAPI *SymFunctionTableAccess64_t)(HANDLE hProcess, DWORD64 AddrBase);
typedef BOOL (WINAPI *StackWalk64_t)(DWORD MachineType, HANDLE hProcess, HANDLE hThread, LPSTACKFRAME64 StackFrame,
	PVOID ContextRecord, PREAD_PROCESS_MEMORY_ROUTINE64 ReadMemoryRoutine,
	PFUNCTION_TABLE_ACCESS_ROUTINE64 FunctionTableAccessRoutine, PGET_MODULE_BASE_ROUTINE64 GetModuleBaseRoutine,
	PTRANSLATE_ADDRESS_ROUTINE64 TranslateAddress);
typedef BOOL (WINAPI *MiniDumpWriteDump_t)(HANDLE hProcess, DWORD ProcessId, HANDLE hFile, MINIDUMP_TYPE DumpType,
	PMINIDUMP_EXCEPTION_INFORMATION ExceptionParam, PMINIDUMP_USER_STREAM_INFORMATION UserStreamParam,
	PMINIDUMP_CALLBACK_INFORMATION CallbackParam);

SymInitialize_t SymInitializePtr = nullptr;
SymCleanup_t SymCleanupPtr = nullptr;
SymLoadModule_t SymLoadModulePtr = nullptr;
SymGetModuleBase_t SymGetModuleBasePtr = nullptr;
SymUnloadModule_t SymUnloadModulePtr = nullptr;
SymGetSymFromAddr_t SymGetSymFromAddrPtr = nullptr;
SymGetLineFromAddr_t SymGetLineFromAddrPtr = nullptr;
SymSetOptions_t SymSetOptionsPtr = nullptr;
SymFunctionTableAccess_t SymFunctionTableAccessPtr = nullptr;
StackWalk_t StackWalkPtr = nullptr;
SymGetModuleBase64_t SymGetModuleBase64Ptr = nullptr;
SymLoadModule64_t SymLoadModule64Ptr = nullptr;
SymGetSymFromAddr64_t SymGetSymFromAddr64Ptr = nullptr;
SymGetLineFromAddr64_t SymGetLineFromAddr64Ptr = nullptr;
SymFunctionTableAccess64_t SymFunctionTableAccess64Ptr = nullptr;
StackWalk64_t StackWalk64Ptr = nullptr;
MiniDumpWriteDump_t MiniDumpWriteDumpPtr = nullptr;

typedef std::set<HANDLE, std::less<HANDLE>, stl::system_allocator<HANDLE> > Processes;

// Is created on the first load, because the loader can be used before static initialization of this file is done.
Processes* InitializedProcesses = nullptr;
HMODULE Module = HMODULE(nullptr);
int ReferenceCount = 0;
bool Failed = false;
bool LoadedFromSystem = false;

// Uses the plain Windows critical section, because it must not allocate with new.
class CriticalSection
{
public:
	CriticalSection() { ::InitializeCriticalSection(&m_criticalSection); }

	void lock() { ::EnterCriticalSection(&m_criticalSection); }
	void unlock() { ::LeaveCriticalSection(&m_criticalSection); }

private:
	CRITICAL_SECTION m_criticalSection;
};

// Is constructed on first use, because the loader can be used during the static initialization of
// other files. Is never destroyed, so that it stays usable during the static destruction too.
lazy_static<CriticalSection> Lock;

class ScopedLock
{
public:
	ScopedLock() { Lock.get().lock(); }
	~ScopedLock() { Lock.get().unlock(); }
};

#define DBGHELP_RESOLVE(name) \
	name##Ptr = reinterpret_cast<name##_t>(::GetProcAddress(Module, #name))

static void resolveAll()
{
	DBGHELP_RESOLVE(SymInitialize);
	DBGHELP_RESOLVE(SymCleanup);
	DBGHELP_RESOLVE(SymLoadModule);
	DBGHELP_RESOLVE(SymGetModuleBase);
	DBGHELP_RESOLVE(SymUnloadModule);
	DBGHELP_RESOLVE(SymGetSymFromAddr);
	DBGHELP_RESOLVE(SymGetLineFromAddr);
	DBGHELP_RESOLVE(SymSetOptions);
	DBGHELP_RESOLVE(SymFunctionTableAccess);
	DBGHELP_RESOLVE(StackWalk);
	DBGHELP_RESOLVE(SymGetModuleBase64);
	DBGHELP_RESOLVE(SymLoadModule64);
	DBGHELP_RESOLVE(SymGetSymFromAddr64);
	DBGHELP_RESOLVE(SymGetLineFromAddr64);
	DBGHELP_RESOLVE(SymFunctionTableAccess64);
	DBGHELP_RESOLVE(StackWalk64);
	DBGHELP_RESOLVE(MiniDumpWriteDump);
}
#undef DBGHELP_RESOLVE

static void freeResources()
{
	while (!InitializedProcesses->empty())
	{
		DbgHelp::SymCleanup(*InitializedProcesses->begin());
	}

	if (Module != HMODULE(nullptr))
	{
		::FreeLibrary(Module);
		Module = HMODULE(nullptr);
	}

	SymInitializePtr = nullptr;
	SymCleanupPtr = nullptr;
	SymLoadModulePtr = nullptr;
	SymGetModuleBasePtr = nullptr;
	SymUnloadModulePtr = nullptr;
	SymGetSymFromAddrPtr = nullptr;
	SymGetLineFromAddrPtr = nullptr;
	SymSetOptionsPtr = nullptr;
	SymFunctionTableAccessPtr = nullptr;
	StackWalkPtr = nullptr;
	SymGetModuleBase64Ptr = nullptr;
	SymLoadModule64Ptr = nullptr;
	SymGetSymFromAddr64Ptr = nullptr;
	SymGetLineFromAddr64Ptr = nullptr;
	SymFunctionTableAccess64Ptr = nullptr;
	StackWalk64Ptr = nullptr;
	MiniDumpWriteDumpPtr = nullptr;

	LoadedFromSystem = false;
}

} // namespace


bool DbgHelpLoader::isLoaded()
{
	ScopedLock lock;

	return Module != HMODULE(nullptr);
}

bool DbgHelpLoader::isLoadedFromSystem()
{
	ScopedLock lock;

	return isLoaded() && LoadedFromSystem;
}

bool DbgHelpLoader::isFailed()
{
	ScopedLock lock;

	return Failed;
}

bool DbgHelpLoader::load()
{
	ScopedLock lock;

	if (InitializedProcesses == nullptr)
	{
		// Cannot use new/delete here when this is loaded during game memory initialization.
		void* p = ::GlobalAlloc(GMEM_FIXED, sizeof(Processes));
		if (p == nullptr)
			return false;
		InitializedProcesses = new (p) Processes();
	}

	// Always increment the reference count.
	++ReferenceCount;

	// Optimization: return early if it failed before.
	if (Failed)
		return false;

	// Return early if someone else already loaded it.
	if (ReferenceCount > 1)
		return true;

	// Try load dbghelp.dll from the system directory first.
	char dllFilename[MAX_PATH];
	const UINT dllFilenameLen = ::GetSystemDirectoryA(dllFilename, sizeof(dllFilename));
	if (dllFilenameLen == 0 || dllFilenameLen >= sizeof(dllFilename) ||
		strlcat(dllFilename, "\\dbghelp.dll", sizeof(dllFilename)) >= sizeof(dllFilename)) {
		Failed = true;
		return false;
	}

	Module = ::LoadLibraryA(dllFilename);
	if (Module == HMODULE(nullptr))
	{
		// Not found. Try load dbghelp.dll from the work directory.
		Module = ::LoadLibraryA("dbghelp.dll");
		if (Module == HMODULE(nullptr))
		{
			Failed = true;
			return false;
		}
	}
	else
	{
		LoadedFromSystem = true;
	}

	resolveAll();

	if (SymInitializePtr == nullptr || SymCleanupPtr == nullptr)
	{
		freeResources();
		Failed = true;
		return false;
	}

	return true;
}

void DbgHelpLoader::unload()
{
	ScopedLock lock;

	if (InitializedProcesses == nullptr)
		return;

	if (--ReferenceCount != 0)
		return;

	freeResources();
	Failed = false;

	InitializedProcesses->~Processes();
	::GlobalFree(InitializedProcesses);
	InitializedProcesses = nullptr;
}


// The dbghelp functions below stand in for the imports of dbghelp.dll. An unresolved function
// returns a neutral value.

namespace DbgHelp
{

BOOL WINAPI SymInitialize(
	HANDLE hProcess,
	PCSTR UserSearchPath,
	BOOL fInvadeProcess)
{
	ScopedLock lock;

	if (InitializedProcesses == nullptr)
		return FALSE;

	if (InitializedProcesses->find(hProcess) != InitializedProcesses->end())
	{
		// Was already initialized.
		return TRUE;
	}

	if (SymInitializePtr != nullptr)
	{
		if (SymInitializePtr(hProcess, UserSearchPath, fInvadeProcess) != FALSE)
		{
			// Is now initialized.
			InitializedProcesses->insert(hProcess);
			return TRUE;
		}
	}

	return FALSE;
}

BOOL WINAPI SymCleanup(
	HANDLE hProcess)
{
	ScopedLock lock;

	if (InitializedProcesses == nullptr)
		return FALSE;

	if (stl::find_and_erase(*InitializedProcesses, hProcess))
	{
		if (SymCleanupPtr != nullptr)
		{
			return SymCleanupPtr(hProcess);
		}
	}

	return FALSE;
}

DWORD WINAPI SymLoadModule(
	HANDLE hProcess,
	HANDLE hFile,
	PCSTR ImageName,
	PCSTR ModuleName,
	DWORD BaseOfDll,
	DWORD SizeOfDll)
{
	ScopedLock lock;

	if (SymLoadModulePtr != nullptr)
		return SymLoadModulePtr(hProcess, hFile, ImageName, ModuleName, BaseOfDll, SizeOfDll);

	return 0;
}

DWORD WINAPI SymGetModuleBase(
	HANDLE hProcess,
	DWORD dwAddr)
{
	ScopedLock lock;

	if (SymGetModuleBasePtr != nullptr)
		return SymGetModuleBasePtr(hProcess, dwAddr);

	return 0u;
}

BOOL WINAPI SymUnloadModule(
	HANDLE hProcess,
	DWORD BaseOfDll)
{
	ScopedLock lock;

	if (SymUnloadModulePtr != nullptr)
		return SymUnloadModulePtr(hProcess, BaseOfDll);

	return FALSE;
}

BOOL WINAPI SymGetSymFromAddr(
	HANDLE hProcess,
	DWORD dwAddr,
	PDWORD pdwDisplacement,
	PIMAGEHLP_SYMBOL Symbol)
{
	ScopedLock lock;

	if (SymGetSymFromAddrPtr != nullptr)
		return SymGetSymFromAddrPtr(hProcess, dwAddr, pdwDisplacement, Symbol);

	return FALSE;
}

BOOL WINAPI SymGetLineFromAddr(
	HANDLE hProcess,
	DWORD dwAddr,
	PDWORD pdwDisplacement,
	PIMAGEHLP_LINE Line)
{
	ScopedLock lock;

	if (SymGetLineFromAddrPtr != nullptr)
		return SymGetLineFromAddrPtr(hProcess, dwAddr, pdwDisplacement, Line);

	return FALSE;
}

DWORD WINAPI SymSetOptions(
	DWORD SymOptions)
{
	ScopedLock lock;

	if (SymSetOptionsPtr != nullptr)
		return SymSetOptionsPtr(SymOptions);

	return 0u;
}

PVOID WINAPI SymFunctionTableAccess(
	HANDLE hProcess,
	DWORD AddrBase)
{
	ScopedLock lock;

	if (SymFunctionTableAccessPtr != nullptr)
		return SymFunctionTableAccessPtr(hProcess, AddrBase);

	return nullptr;
}

BOOL WINAPI StackWalk(
	DWORD MachineType,
	HANDLE hProcess,
	HANDLE hThread,
	LPSTACKFRAME StackFrame,
	PVOID ContextRecord,
	PREAD_PROCESS_MEMORY_ROUTINE ReadMemoryRoutine,
	PFUNCTION_TABLE_ACCESS_ROUTINE FunctionTableAccessRoutine,
	PGET_MODULE_BASE_ROUTINE GetModuleBaseRoutine,
	PTRANSLATE_ADDRESS_ROUTINE TranslateAddress)
{
	ScopedLock lock;

	if (StackWalkPtr != nullptr)
		return StackWalkPtr(MachineType, hProcess, hThread, StackFrame, ContextRecord, ReadMemoryRoutine,
			FunctionTableAccessRoutine, GetModuleBaseRoutine, TranslateAddress);

	return FALSE;
}

DWORD64 WINAPI SymGetModuleBase64(
	HANDLE hProcess,
	DWORD64 dwAddr)
{
	ScopedLock lock;

	if (SymGetModuleBase64Ptr != nullptr)
		return SymGetModuleBase64Ptr(hProcess, dwAddr);

	return 0u;
}

DWORD64 WINAPI SymLoadModule64(
	HANDLE hProcess,
	HANDLE hFile,
	PCSTR ImageName,
	PCSTR ModuleName,
	DWORD64 BaseOfDll,
	DWORD SizeOfDll)
{
	ScopedLock lock;

	if (SymLoadModule64Ptr != nullptr)
		return SymLoadModule64Ptr(hProcess, hFile, ImageName, ModuleName, BaseOfDll, SizeOfDll);

	return 0u;
}

BOOL WINAPI SymGetSymFromAddr64(
	HANDLE hProcess,
	DWORD64 Address,
	PDWORD64 Displacement,
	PIMAGEHLP_SYMBOL64 Symbol)
{
	ScopedLock lock;

	if (SymGetSymFromAddr64Ptr != nullptr)
		return SymGetSymFromAddr64Ptr(hProcess, Address, Displacement, Symbol);

	return FALSE;
}

BOOL WINAPI SymGetLineFromAddr64(
	HANDLE hProcess,
	DWORD64 dwAddr,
	PDWORD pdwDisplacement,
	PIMAGEHLP_LINE64 Line)
{
	ScopedLock lock;

	if (SymGetLineFromAddr64Ptr != nullptr)
		return SymGetLineFromAddr64Ptr(hProcess, dwAddr, pdwDisplacement, Line);

	return FALSE;
}

PVOID WINAPI SymFunctionTableAccess64(
	HANDLE hProcess,
	DWORD64 AddrBase)
{
	ScopedLock lock;

	if (SymFunctionTableAccess64Ptr != nullptr)
		return SymFunctionTableAccess64Ptr(hProcess, AddrBase);

	return nullptr;
}

BOOL WINAPI StackWalk64(
	DWORD MachineType,
	HANDLE hProcess,
	HANDLE hThread,
	LPSTACKFRAME64 StackFrame,
	PVOID ContextRecord,
	PREAD_PROCESS_MEMORY_ROUTINE64 ReadMemoryRoutine,
	PFUNCTION_TABLE_ACCESS_ROUTINE64 FunctionTableAccessRoutine,
	PGET_MODULE_BASE_ROUTINE64 GetModuleBaseRoutine,
	PTRANSLATE_ADDRESS_ROUTINE64 TranslateAddress)
{
	ScopedLock lock;

	if (StackWalk64Ptr != nullptr)
		return StackWalk64Ptr(MachineType, hProcess, hThread, StackFrame, ContextRecord, ReadMemoryRoutine,
			FunctionTableAccessRoutine, GetModuleBaseRoutine, TranslateAddress);

	return FALSE;
}

BOOL WINAPI MiniDumpWriteDump(
	HANDLE hProcess,
	DWORD ProcessId,
	HANDLE hFile,
	MINIDUMP_TYPE DumpType,
	PMINIDUMP_EXCEPTION_INFORMATION ExceptionParam,
	PMINIDUMP_USER_STREAM_INFORMATION UserStreamParam,
	PMINIDUMP_CALLBACK_INFORMATION CallbackParam)
{
	ScopedLock lock;

	if (MiniDumpWriteDumpPtr != nullptr)
		return MiniDumpWriteDumpPtr(hProcess, ProcessId, hFile, DumpType, ExceptionParam, UserStreamParam, CallbackParam);

	return FALSE;
}

} // namespace DbgHelp
