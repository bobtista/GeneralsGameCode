# macOS Native Build — Status & Resume Notes

**Last updated:** 2026-04-28
**Working directory:** `/Users/bobbybattista/Code/GeneralsGameCode-work`
**Active branch:** `bobtista/build/macos-bundle`
**Target:** GeneralsMD (Zero Hour) on Apple Silicon arm64

## Current status

| Stage                  | Status | Notes                                                                                  |
| ---------------------- | ------ | -------------------------------------------------------------------------------------- |
| Configure (CMake)      | OK     | `macos-generalsmd-sdl3-bgfx` preset                                                    |
| Compile + link         | OK     | `generalszh` Mach-O 64-bit arm64, ~16 MB                                               |
| Engine init            | OK     | Subsystems initialise; INI/Big files load                                              |
| Headless mode          | OK     | `-headless` survives 10s+ runtime smoke                                                |
| Window appears         | OK     | SDL3 + Metal-backed CAMetalLayer surface                                               |
| First-frame Metal draw | BLOCK  | Deterministic SIGSEGV inside Apple AGX driver (`AGCDeserializedReply` ctor) on submit  |

Runtime smoke shows the engine is fully alive — the wall is bgfx → Metal pipeline-state compile when the first internal blit/clear program runs.

## Goal

Build and run a Mac-native (Apple Silicon arm64) GeneralsMD. Replace Win-only deps with cross-platform equivalents:

- DX8 → bgfx (Metal backend)
- Win32 windowing/input → SDL3
- Miles Sound System → OpenAL
- Bink → FFmpeg
- Win registry → INI file
- Win `My Documents` → `~/Library/Application Support/`

Inspired by `fbraz3/GeneralsX`, but uses **bgfx** (native Metal) rather than DXVK (translation layer).

## Branch structure

```
superhackers/main
├─ bobtista/feat/phase4-bgfx-cutover      (bgfx core renderer — OWNED BY WINDOWS MACHINE; do not push from Mac)
├─ bobtista/feat/sdl3-platform            (SDL3 windowing/input — based on superhackers/main)
├─ bobtista/feat/openal-audio             (OpenAL backend — based on superhackers/main)
├─ bobtista/feat/ffmpeg-video             (FFmpeg + macOS discovery — based on superhackers/main)
├─ bobtista/feat/registry-unix            (RegistryIni helper + GeneralsMD-only UNIX branch)
├─ bobtista/feat/userdata-path-unix       (GlobalData UNIX branch — sits on registry-unix)
├─ bobtista/feat/bgfx-macos-renderer      (phase4 + sdl3 + macOS renderer glue)
└─ bobtista/build/macos-bundle            (full integration + deploy scripts) ← active
```

**Pre-decouple backup tags on origin:**
- `bobtista/backup/sdl3-platform-bgfx-stack-pre-decouple`
- `bobtista/backup/openal-audio-bgfx-stack-pre-decouple`
- `bobtista/backup/ffmpeg-video-bgfx-stack-pre-decouple`
- `bobtista/backup/bgfx-macos-renderer-pre-decouple`
- `bobtista/backup/macos-bundle-pre-decouple`

**Critical workflow rules:**
- Latest main is `superhackers/main`, not `origin/main`.
- All macOS port work is **GeneralsMD-only** — never mirror to `Generals/` (basegam).
- `phase4-bgfx-cutover` is being worked on a Windows machine; **never force-push it** from here.
- Branches go to `origin` (fork). PRs target `superhackers/main`.

## How to build / deploy / run

### One-shot rebuild + deploy

```sh
scripts/build/macos/build-macos-generalsmd.sh
scripts/build/macos/deploy-macos-generalsmd.sh
```

### Get game data (sparse-clone of GeneralsGamePatch loose-file mirror)

```sh
scripts/build/macos/fetch-game-data.sh
```

Pulls `Patch104pZH/GameFilesOriginalZH/{Data,Window,Art}` into the runtime dir.

### `.big` files

The user has the original install on PC and copied `INIZH.big`, `Maps.big`, etc. into `~/TheSuperHackers/GeneralsZH/`. Loose `Data/`, `Window/`, `Art/` from the patch repo coexist with `.big` files.

### Run

```sh
cd ~/TheSuperHackers/GeneralsZH
./generalszh                     # full run
./generalszh -headless           # init + tick loop without window/render (works today)
GGC_BGFX_DEBUG=1 ./generalszh    # bgfx verbose logs to stderr
```

Logs:
- `/tmp/ggc-run.log` — engine log
- `~/Library/Logs/DiagnosticReports/*.ips` — Apple crash reports

## Runtime layout

```
~/TheSuperHackers/GeneralsZH/
├── generalszh                      (Mach-O arm64)
├── *.dylib                         (bgfx, SDL3, openal-soft, ffmpeg ... resolved via @rpath)
├── Data/                           (loose INIs from GameFilesOriginalZH)
├── Window/
├── Art/
├── *.big                           (INIZH, Maps, AudioZH, etc.)
└── (RegistryIni and user data live in ~/Library/Application Support/TheSuperHackers/)
```

## What's blocked: bgfx Metal first-frame submit

### Symptom

First call into `bgfx::frame()` after the renderer is up reliably faults inside `AGCDeserializedReply`'s constructor in Apple's AGX Metal driver while compiling/loading a render pipeline state for `BlitFastClearProgramVariant` (an internal bgfx clear shader).

```
EXC_BAD_ACCESS (SIGSEGV)  KERN_INVALID_ADDRESS at 0xce845bfa4837b9ee
Frames (top of stack):
  AGCDeserializedReply::AGCDeserializedReply(...)
  AGCDecodeAndStripReply / agc_internal_pipeline_compile
  -[_MTLRenderPipelineState newRenderPipelineStateWithDescriptor:...]
  bgfx::mtl::RendererContextMtl::createRenderPipelineState(...)
  bgfx::mtl::RendererContextMtl::submitBlit(...)
  bgfx::mtl::RendererContextMtl::submit(...)   ← first frame
  bgfx::frame()
```

Fault address is **identical across runs** (`0xce845bfa4837b9ee`) — looks like a poisoned/uninitialised pointer being dereferenced rather than memory pressure or race.

### What we tried

- `SDL_WINDOW_METAL` flag on window creation so bgfx Metal sees a CAMetalLayer-backed view (kept).
- Passed NSWindow vs. NSView `[contentView]` via `objc_msgSend` dlsym (reverted; no behaviour change).
- Built with `bgfx::Init::debug = true` and `BGFX_DEBUG_PROFILER` — no extra signal in log.
- Confirmed headless mode (no bgfx init) is stable.

### Likely directions for next session

1. Bump bgfx submodule from pinned commit `668550dc7c47c71860a39c5ef4c162e79294c93f` to current; the Metal backend has been actively patched.
2. Build a minimal bgfx sample (bgfx's own `00-helloworld`) against the same SDK to confirm bgfx Metal works on this hardware.
3. Disable the internal `BlitFastClear` path or hand-build a simpler clear-screen pipeline to bisect.
4. Run under Metal API validation (Xcode env `MTL_DEBUG_LAYER=1`, `MTL_SHADER_VALIDATION=1`).
5. Inspect `MTLRenderPipelineDescriptor` bgfx hands the driver — pixel formats, attachment count, stencil format may not match the drawable.

## Key files & directories

### Compat shim layer
- `Core/Libraries/Source/WWVegas/compat/win32_shims/`
  - `windows.h` — master shim (HRESULT, LPARAM, OSVERSIONINFO, MessageBox, _stricmp, etc.)
  - `winsock.h` — SOCKET=int, closesocket→close, WSA* macros
  - `file_compat.h` — FindFirstFile/FindNextFile via dirent.h (from GeneralsX)
  - `process.h`, `io.h`, `direct.h`, `sys/timeb.h`, `new.h`, `mbstring.h`, `mmsystem.h`
  - `objbase.h` — IID_IUnknown + GUID `operator==`
  - `dinput.h`, `d3d8_iids.h`, `windowsx.h`
- `Core/Libraries/Source/WWVegas/CMakeLists.txt` — adds `compat/win32_shims` to `core_wwcommon` + `core_wwvegas` `BEFORE INTERFACE` so shims win over dx8-SDK headers.

### WWLib / WWDownload / debug / profile non-Win wiring
- `Core/Libraries/Source/WWVegas/WWLib/CMakeLists.txt` — WIN32-conditional PCH; `<windows.h>` first on non-Win.
- `Core/Libraries/Source/WWVegas/WWLib/registry_unix_stub.cpp` — minimal RegistryClass returning defaults (DX8Wrapper / W3DDisplay link target).
- `Core/Libraries/Source/WWVegas/WWDownload/{CMakeLists.txt,WWDownloadStub.cpp}` — Win-only Download/FTP, stub on non-Win.
- `Core/Libraries/Source/debug/CMakeLists.txt` — entire `core_debug` lib gated on WIN32 (per GeneralsX).
- `Core/Libraries/Source/profile/{profile.cpp,profile_funclevel.cpp,profile_highlevel.cpp,profile_funclevel.h}` — GeneralsX-style `#ifdef _WIN32` around HGLOBAL / QueryPerformanceCounter / wsprintf / CreateEvent.

### Pinch-points that bit us
- `Core/GameEngine/Include/GameClient/GameWindow.h` — `WindowMsgData` typedef changed to `uintptr_t`; one swap removes ~30 cast errors.
- `GeneralsMD/Code/GameEngine/Include/Precompiled/PreRTS.h` — wraps `<atlbase.h>`, `<excpt.h>`, `<imagehlp.h>`, `<lmcons.h>`, `<ocidl.h>`, `<shellapi.h>`, `<shlobj.h>`, `<shlguid.h>`, `<snmp.h>`, `<tchar.h>`, `<vfw.h>`, `<winerror.h>`, `<wininet.h>`, `<winreg.h>`, `<dinput.h>` in `#ifdef _WIN32`.
- `GeneralsMD/Code/GameEngine/Include/GameClient/KeyDefs.h` — unguarded `#include <dinput.h>` resolves to compat shim on non-Win.
- `Core/GameEngine/Source/Common/INI/INI.cpp` — `#define USE_STD_FROM_CHARS_PARSING 1` only when `__cplusplus >= 201611L && !defined(__APPLE__)` (Apple libc++ lacks float `from_chars`).
- `Core/GameEngineDevice/Source/StdDevice/Common/StdLocalFileSystem.cpp` — `for (auto p : path)` (was `auto&`); libc++ iterator yields a temporary.
- `Dependencies/Utility/Utility/string_compat.h` — `_strlwr` is `extern "C" inline` to match gamespy's extern "C" decl.
- `Dependencies/Utility/Utility/endian_compat.h` — Apple branch uses `uint16_t/uint32_t/uint64_t` (not CoreServices `UInt*`).

### Apple-specific Win body wraps
- `Core/GameEngine/Source/Common/WorkerProcess.cpp` — entire Win body in `#ifdef _WIN32`; non-Win stub class.
- `Core/GameEngine/Source/GameClient/GUI/IMEManager.cpp` — same pattern.
- `Core/GameEngine/Source/GameNetwork/WOLBrowser/WebBrowser.cpp` + `WebBrowser.h` — non-Win stub `class WebBrowser : public SubsystemInterface` with virtual createBrowserWindow / closeBrowserWindow / makeNewURL / findURL; Win path keeps original ATL.
- `GeneralsMD/Code/GameEngineDevice/Source/W3DDevice/GameClient/W3DWebBrowser.cpp` — same.
- `GeneralsMD/Code/GameEngine/Source/Common/GameEngine.cpp` — `_Module.Init()` / `_Module.Term()` calls in `#ifdef _WIN32`.

### Networking compat
- `Transport.cpp`, `udp.cpp`, `PeerThread.cpp` — `from.sin_addr.S_un.S_addr` → `from.sin_addr.s_addr`; `recvfrom` / `getsockname` cast `&len` to `(socklen_t *)&len`; `EWOULDBLOCK` case wrapped in `#if EWOULDBLOCK != EAGAIN`.

### WW3D2
- `Core/Libraries/Source/WWVegas/WW3D2/render2dsentence.cpp` — non-Win `Store_GDI_Char` returns synthetic CharData with `Buffer = nullptr`; `Blit_Char` got a null-buffer guard.
- `Core/Libraries/Source/WWVegas/WW3D2/BgfxBackend.cpp` — `FindWindowW`/`SW_SHOWNA`/`SetForegroundWindow`/`SetFocus` block in `#ifdef _WIN32`; bgfx callback writes via `fprintf(stderr)`; `initArgs.debug = std::getenv("GGC_BGFX_DEBUG") != nullptr`; `GetNativeWindowHandle` returns NSWindow pointer.
- `W3DMouse.cpp` — `::SetCursor(nullptr)` etc. qualified.
- `W3DShaderManager.cpp` — DriverVersion union access in `#ifdef _WIN32`; non-Win uses `(High << 32) | Low`.

### Entry point
- `GeneralsMD/Code/Main/SDL3Main.cpp` — critical init order:
  1. `#include "windows.h"` (compat) for AsciiString._stricmp
  2. `SDL_CreateWindow(... SDL_WINDOW_RESIZABLE | SDL_WINDOW_METAL)`
  3. `ApplicationHWnd = TheSDL3Window`
  4. `TheVersion = NEW Version; TheVersion->setVersion(...)`
  5. `CommandLine::parseCommandLineForStartup()`
  6. `GameMain()`
  Also defines stub symbols the Win build expects: `g_strFile`, `g_csfFile`, `g_LastErrorDump`, `FillStackAddresses`, `StackDumpFromAddresses`, `StackDump`, `GetFunctionDetails`, `DumpExceptionInfo`, `OSDisplaySetBusyState`.

### Build / scripts
- `scripts/build/macos/build-macos-generalsmd.sh` — drives the configure + ninja build.
- `scripts/build/macos/deploy-macos-generalsmd.sh` — flat-dir deploy to `~/TheSuperHackers/GeneralsZH/` (no .app bundle).
- `scripts/build/macos/fetch-game-data.sh` — sparse-clone GeneralsGamePatch loose-file game data.
- `cmake/dx8.cmake` — `dinput8 dxguid` link only on WIN32 (still pulls headers).
- `cmake/config-memory.cmake` — `RTS_CRASHDUMP_ENABLE` forced OFF on non-Win.
- `CMakeLists.txt` — `dx8.cmake` is included whenever `GGC_BGFX_STANDALONE` is set, even off-Win, because the bgfx renderer still uses legacy d3dx8 math/types.

## GeneralsX patterns we adopted

- `core_debug` library wrapped entirely in `if(WIN32)` at CMake level.
- `core_profile_legacy` patched with `#ifdef _WIN32` guards rather than reimplemented.
- `file_compat.h` lifted into our compat layer.
- `_INT64_TYPES_DEFINED` guard pattern — chose `#define __int64 long long` over typedef so `unsigned __int64` syntax keeps working.
- `WindowMsgData = uintptr_t` instead of mass `(intptr_t)` casts at call sites.

## Recent commits on this branch (top → bottom = newest first)

```
build(macos): expose dx8 headers on non-Win, disable crashdumps, add fetch-game-data
feat(generalsmd-main): wire SDL3 init order, Version, CommandLine, bootstrap stubs
fix(generalsmd-device): non-Win W3DDevice + WW3D2 fork adjustments
fix(generalsmd-engine): PreRTS Win headers, KeyDefs dinput shim, 64-bit pointer casts
fix(ww3d2): non-Windows backend wiring, GDI font stub, BgfxBackend diagnostics
fix(gameenginedevice): adapt Win-only call sites for non-Windows toolchain
fix(core): guard Win-only paths and 64-bit cast pinch-points across GameEngine
fix(compat): align Utility/* compat headers with Apple/Clang toolchain
build(wwvegas): gate Win-only WWLib/WWDownload/debug/profile sources on non-Win
build(compat): add Win32 shim headers for non-Windows builds
build: add flat-directory deploy scripts for GeneralsMD macOS
merge: bring in registry-unix + userdata-path-unix
…
```

## Next session — concrete focus

**Single-issue debug session: bgfx Metal `AGCDeserializedReply` fault on first frame.**

Order of attack:

1. Reproduce against a fresh checkout, confirm fault address still identical → confirms determinism.
2. Bump `bgfx` to head of master in the FetchContent declaration; rebuild; rerun.
3. If still broken, build `bgfx/examples/00-helloworld` against the same SDK and run — isolates engine vs. bgfx-Metal vs. driver.
4. Run with `MTL_DEBUG_LAYER=1 MTL_SHADER_VALIDATION=1` and capture a Metal frame in Xcode to inspect the descriptor bgfx feeds the driver.
5. As a fallback, try `BGFX_RESET_VSYNC | BGFX_RESET_FLIP_AFTER_RENDER` and disable any `BGFX_CAPS_BLIT`-dependent path.

**Out of scope until rendering is up:** OpenAL audio runtime testing, FFmpeg cinematics, gameplay smoke. The blocker is render-side only.

## Don't-touch list

- `Generals/` (basegam) — never mirror macOS port changes here. Memory: `feedback_macos_work_generalsmd_only.md`.
- `bobtista/feat/phase4-bgfx-cutover` — owned by Windows machine; do not force-push. Memory: `feedback_phase4_bgfx_windows_owned.md`.
- Open PRs — never close, edit body/title, or comment without explicit user approval. Memories: `feedback_no_close_prs.md`, `feedback_pr_comments.md`, `feedback_pr_body_edits.md`.
