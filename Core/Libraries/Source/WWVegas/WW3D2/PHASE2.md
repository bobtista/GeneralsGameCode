# Render Backend Phase 2 — Add bgfx and Diligent backends

**Branch:** `bobtista/feat/phase2-render-backends`
**Base:** `bobtista/refactor/render-backend-interface` (Phase 1)
**Status:** scaffolding complete, pending Windows build verification

See [RENDER_BACKEND.md](RENDER_BACKEND.md) for the full multi-phase plan.

## Goal

Add two parallel implementations of `IRenderBackend` — `BgfxBackend` and `DiligentBackend` — so the game can be built with either as the rendering backend via a CMake flag, while DX8 remains the default and the reference implementation.

**End state:** `cmake -DGGC_RENDER_BACKEND=bgfx` produces a `generalszh.exe` that builds, links, launches, and displays a cleared window (plus whatever the one migrated call site in `W3DStatusCircle::Render()` can draw, which is very little). Same for `diligent`. With the default `dx8` value the build is byte-identical to today's main.

## Non-goals

- **Full backend implementations.** Most virtual methods on `BgfxBackend` and `DiligentBackend` are stubbed (no-op or debug trace) in Phase 2. Actually rendering the game through a non-DX8 backend requires Phase 3's subsystem-by-subsystem migration, because most of the engine is still using `DX8Wrapper::` statics directly.
- **Migrating additional call sites.** That's Phase 3. Phase 2 only adds backend scaffolding.
- **Making the game visually functional under bgfx/Diligent.** It won't be. Most draw calls will no-op. This is expected and correct for Phase 2.
- **VC6 compatibility for bgfx/Diligent.** The new backend code requires MSVC 2022 and C++17. The DX8 backend stays VC6-compatible. `IRenderBackend.h` is still C++98-safe because the new backends include it from their own C++17 translation units.
- **Runtime backend selection.** Phase 2 is compile-time only. A single build produces a single backend-flavored `.exe`.

## Design

### `GGC_RENDER_BACKEND` CMake option

A string option with validated values:

```cmake
set(GGC_RENDER_BACKEND "dx8" CACHE STRING "Rendering backend: dx8 (default), bgfx, or diligent")
set_property(CACHE GGC_RENDER_BACKEND PROPERTY STRINGS dx8 bgfx diligent)
```

It emits one preprocessor define:

```cmake
if(GGC_RENDER_BACKEND STREQUAL "dx8")
    target_compile_definitions(... PUBLIC GGC_RENDER_BACKEND_DX8=1)
elseif(GGC_RENDER_BACKEND STREQUAL "bgfx")
    target_compile_definitions(... PUBLIC GGC_RENDER_BACKEND_BGFX=1)
elseif(GGC_RENDER_BACKEND STREQUAL "diligent")
    target_compile_definitions(... PUBLIC GGC_RENDER_BACKEND_DILIGENT=1)
else()
    message(FATAL_ERROR "Invalid GGC_RENDER_BACKEND: ${GGC_RENDER_BACKEND}")
endif()
```

and conditionally includes the backend dependency module:

```cmake
if(GGC_RENDER_BACKEND STREQUAL "bgfx")
    include(cmake/bgfx.cmake)
elseif(GGC_RENDER_BACKEND STREQUAL "diligent")
    include(cmake/diligent.cmake)
endif()
```

The DX8 backend has no dependency module — it uses the existing `cmake/dx8.cmake`.

### Dependency management via `FetchContent`

bgfx.cmake and DiligentCore are pulled via `FetchContent_Declare` + `FetchContent_MakeAvailable`, matching the project's existing pattern (see `cmake/dx8.cmake`). This keeps them out of the source tree, pins them to a specific SHA for reproducibility, and avoids `.gitmodules` bloat.

Pinned SHAs (captured from the spike):

- **bgfx.cmake**: `bkaradzic/bgfx.cmake@668550d` (nested: bgfx@9c98438, bx@cac72f6, bimg@9114b47)
- **DiligentCore**: `DiligentGraphics/DiligentCore@296ca9a89`

### Compile-time selection in `RenderBackend.cpp`

The factory function becomes:

```cpp
void Init_Render_Backend()
{
    if (g_renderBackend != nullptr)
    {
        return;
    }
#if defined(GGC_RENDER_BACKEND_BGFX)
    g_renderBackend = new BgfxBackend();
#elif defined(GGC_RENDER_BACKEND_DILIGENT)
    g_renderBackend = new DiligentBackend();
#else
    g_renderBackend = new DX8Backend();
#endif
}
```

### Conditional source inclusion in WW3D2 CMakeLists

The new backend sources are only compiled when their respective flag is set. This matters because:

- bgfx and Diligent require MSVC 2022 + C++17, which the VC6 build cannot provide. If those files always compiled, VC6 builds would break.
- Pulling in the full bgfx or Diligent dependency adds significant build time that DX8-only users shouldn't pay.
- Cross-platform builds only need the backend they're targeting.

Implementation:

```cmake
# In Core/Libraries/Source/WWVegas/WW3D2/CMakeLists.txt
if(GGC_RENDER_BACKEND STREQUAL "bgfx")
    list(APPEND WW3D2_SRC
        BgfxBackend.cpp
        BgfxBackend.h
    )
endif()
if(GGC_RENDER_BACKEND STREQUAL "diligent")
    list(APPEND WW3D2_SRC
        DiligentBackend.cpp
        DiligentBackend.h
    )
endif()
```

And the link step conditionally pulls in the backend's library targets on top of `z_ww3d2`:

```cmake
if(GGC_RENDER_BACKEND STREQUAL "bgfx")
    target_link_libraries(z_ww3d2 PRIVATE bgfx bx bimg)
elseif(GGC_RENDER_BACKEND STREQUAL "diligent")
    target_link_libraries(z_ww3d2 PRIVATE
        Diligent-GraphicsEngineD3D11-static
        Diligent-Common
        Diligent-GraphicsTools
    )
endif()
```

### What the stub backends implement vs stub

Both `BgfxBackend` and `DiligentBackend` implement every virtual method on `IRenderBackend` (~60 methods). The methods split into two categories:

**Real implementation (lifecycle + state queries):**

- `init()` / `shutdown()` — create the device and swapchain, register with the HWND from `DX8Wrapper::_Hwnd`
- `Begin_Scene()` / `End_Scene()` / `Flip_To_Primary()` — frame lifecycle
- `Clear()` — clear the color + depth buffers
- `Set_Viewport()` — set the viewport for subsequent draws
- `Is_Device_Lost()` — device loss state
- `Has_Stencil()` — stencil support query

**Stubbed (no-op or debug trace):**

- All resource creation (`Create_Render_Target`, anything that would need to allocate bgfx/Diligent objects keyed on W3D class identities)
- All state setters that take W3D types (`Set_Shader`, `Set_Material`, `Set_Texture`, etc.)
- All transforms
- All lighting
- All draw calls (`Draw_Triangles`, `Draw_Strip`)
- The programmable pipeline methods (`Set_Vertex_Shader` etc.)
- Render target binding

The stubs do NOT assert or crash. They log a one-time "not implemented" debug message and return. This lets the game boot and run through the render loop without crashing, and lets Phase 3 fill in the stubs one subsystem at a time without coordinating with a monolithic rewrite.

### Why most methods stay stubbed in Phase 2

The real `IRenderBackend` interface takes W3D classes as parameters (`ShaderClass`, `VertexBufferClass`, `IndexBufferClass`, `TextureBaseClass`, `LightClass`, etc.). A real implementation must:

1. Unwrap the W3D object to get raw data (vertex data, state bits, texture pixels)
2. Translate that data to the backend's native representation
3. Cache the translated result keyed on the W3D object identity so the same `ShaderClass` doesn't re-translate every frame

That's a **large** implementation, different for every W3D type, and depends on Phase 3 migration to know which methods each subsystem actually calls. It's much easier to stub in Phase 2 and fill in as Phase 3 progresses, than to try to pre-implement the full surface and then discover that half the methods aren't actually used the way we expected.

## Task list

- [x] **2.0** Write this document
- [x] **2.1** Add `cmake/render-backend.cmake` with the `GGC_RENDER_BACKEND` option and validation
- [x] **2.2** Update `RenderBackend.cpp` to `#if`-select the concrete backend
- [x] **2.3** Add `cmake/bgfx.cmake` with `FetchContent_Declare` for bgfx.cmake
- [x] **2.4** Write `BgfxBackend.{h,cpp}` with stubs (deferred real lifecycle to Phase 3)
- [x] **2.5** Add `cmake/diligent.cmake` with `FetchContent_Declare` for DiligentCore
- [x] **2.6** Write `DiligentBackend.{h,cpp}` with stubs (deferred real lifecycle to Phase 3)
- [x] **2.7** Update this document with completion state + Phase 3 handoff

## What landed in Phase 2

**New files:**

- `cmake/render-backend.cmake` — defines the `GGC_RENDER_BACKEND` string option with validation (must be `dx8`, `bgfx`, or `diligent`), emits the corresponding compile definition into `GGC_RENDER_BACKEND_COMPILE_DEFINE`, conditionally includes the chosen backend's dependency module (`cmake/bgfx.cmake` or `cmake/diligent.cmake`), and fails early if a non-DX8 backend is selected under a VC6 build.
- `cmake/bgfx.cmake` — `FetchContent_Declare` for `bkaradzic/bgfx.cmake@668550dc7c47c71860a39c5ef4c162e79294c93f` with nested submodules (bgfx/bx/bimg) auto-recursed. Sets `BGFX_BUILD_*` options to disable examples, tests, geometry/texture tools, and install, while keeping shaderc.
- `cmake/diligent.cmake` — `FetchContent_Declare` for `DiligentGraphics/DiligentCore@296ca9a891b781a49643948495efcf31b1339f50`. Sets `DILIGENT_NO_*` options to keep only the D3D11 backend and disable D3D12/Vulkan/OpenGL/Metal/WebGPU/Archiver/SuperResolution/Tests.
- `Core/Libraries/Source/WWVegas/WW3D2/BgfxBackend.{h,cpp}` — `BgfxBackend : public IRenderBackend`, implements every virtual method as a no-op or returns a sensible default. Includes `<bgfx/bgfx.h>` and anchors a reference to `bgfx::getCaps` so the linker must resolve bgfx symbols; any misconfigured FetchContent / link setup fails loudly at link time. Constructor logs a one-line warning to stderr on first use. No actual rendering logic.
- `Core/Libraries/Source/WWVegas/WW3D2/DiligentBackend.{h,cpp}` — identical shape to `BgfxBackend`, anchored on `Diligent::GetEngineFactoryD3D11`. Same stub pattern.
- `Core/Libraries/Source/WWVegas/WW3D2/PHASE2.md` — this document.

**Modified files:**

- `CMakeLists.txt` (top level) — adds `include(cmake/render-backend.cmake)` after `config.cmake`, before the subdirectories. Default build stays byte-identical.
- `Core/Libraries/Source/WWVegas/WW3D2/CMakeLists.txt` — conditionally appends the active backend's sources to `corei_ww3d2`, propagates the compile definition via `target_compile_definitions(corei_ww3d2 INTERFACE ...)`, and conditionally links the backend's native libraries (`bgfx bx bimg` or `Diligent-GraphicsEngineD3D11-static Diligent-Common Diligent-GraphicsTools`).
- `Core/Libraries/Source/WWVegas/WW3D2/RenderBackend.cpp` — `Init_Render_Backend()` now uses `#if defined(GGC_RENDER_BACKEND_BGFX)` / `#elif defined(GGC_RENDER_BACKEND_DILIGENT)` / `#else` to pick the concrete backend class at compile time. DX8 is the default fallback.

**Commits on this branch (`bobtista/feat/phase2-render-backends`, based on `bobtista/refactor/render-backend-interface`):**

```
fe9ecc57d feat(ww3d2): add DiligentBackend stub for GGC_RENDER_BACKEND=diligent
8db698c00 build(diligent): add FetchContent module pinning DiligentCore for phase 2
370db0291 feat(ww3d2): add BgfxBackend stub for GGC_RENDER_BACKEND=bgfx
d126c0b9a build(bgfx): add FetchContent module pinning bgfx.cmake for phase 2
d9b8f76cc feat(ww3d2): select render backend at compile time in RenderBackend.cpp
61a375b04 build(ww3d2): add GGC_RENDER_BACKEND compile-time flag
bb3e449c4 docs(ww3d2): add Phase 2 backend-implementation plan
```

## Scope change vs the original Phase 2 plan

The original plan (written before implementation) promised "real lifecycle (init, shutdown, Begin_Scene, End_Scene, Clear, Set_Viewport) plus stubs elsewhere." During implementation, it became clear that even the "real" lifecycle methods can't be implemented meaningfully in Phase 2 because:

1. **HWND plumbing.** The bgfx/Diligent device needs the native window handle, which is owned by `DX8Wrapper` (stored in `DX8Wrapper::_Hwnd`). Phase 2 has no mechanism for passing that HWND into the backend constructor without adding a new virtual method to `IRenderBackend` (`Init(void*, int, int)`) and updating `DX8Backend` to implement it — which is legitimate work but bloats Phase 2 in a way that doesn't buy anything yet.

2. **Coexistence with DX8.** When `GGC_RENDER_BACKEND=bgfx` is selected but Phase 3 migration hasn't happened, 99% of the rendering still goes through `DX8Wrapper::*` statics, which are driving an `IDirect3DDevice8` of their own. A real bgfx swapchain on the same HWND would fight the DX8 swapchain for ownership. The only sensible Phase 2 behavior is for the bgfx backend to NOT create a swapchain at all — let DX8 keep running the show, and let the Phase 1-migrated `W3DStatusCircle` calls silently no-op through the bgfx stubs.

3. **Visual output under bgfx/Diligent in Phase 2 is not meaningful.** Only one call site (`W3DStatusCircle::Render`) is migrated. Even if `Clear()` and `Begin_Scene()` were real on bgfx, the scene would still be drawn by DX8. The user-visible result of "cleared bgfx screen" vs "DX8 rendered scene" would be confusing and pointless.

**Corrected Phase 2 exit criterion:** `cmake -DGGC_RENDER_BACKEND=bgfx` fetches bgfx via FetchContent, builds the bgfx libraries, compiles `BgfxBackend.cpp`, links `generalszh.exe` successfully. Launching the game boots into DX8-driven rendering exactly as it does today (because 99% of rendering is still via DX8Wrapper statics), with `W3DStatusCircle::Render()`'s migrated calls silently no-oping through the bgfx stubs — meaning the status circle won't render but nothing else changes. This is the correct behavior for the current migration state. Same for `diligent`.

## What this does NOT do

- **No real rendering on bgfx or Diligent.** Every backend-owned method is a no-op. Phase 3 migrates subsystems one at a time and fills in the stubs as needed.
- **No init/shutdown parameter passing.** Phase 2 backends don't know about HWND. When the first subsystem needs real GPU resources through a non-DX8 backend (Phase 3), we'll add an `Init(void* hwnd, int w, int h)` virtual method to `IRenderBackend` or similar mechanism, and update `DX8Backend` to implement it (trivially — it already has access to `DX8Wrapper::_Hwnd`).
- **No runtime backend switching.** A single build corresponds to a single backend. Switching requires reconfiguring CMake and rebuilding.
- **No visual output differences with `GGC_RENDER_BACKEND=bgfx` or `=diligent`.** The game runs identically to the DX8 build because 99% of rendering still goes through `DX8Wrapper::*` directly, not through `g_renderBackend->*`. The one migrated call site (`W3DStatusCircle::Render`) silently no-ops through the new backend's stubs — you'll see the game without a status circle when it would otherwise show one. Everything else is untouched.
- **No Linux or macOS builds.** Phase 4 adds those. bgfx and Diligent are both cross-platform-ready, but the rest of WW3D2 and the game still depends on DirectX 8 headers for a lot of things (texture loading, device creation, the 33 leaky files from Phase 1's analysis). Cross-platform is a whole separate phase.

## Windows build verification needed

This branch has NOT been built on any host. Same constraint as Phase 1 — development happens on macOS where DX8/Windows SDK headers are not available. The FetchContent modules WILL clone their dependencies anywhere (macOS included) but the actual compile step requires Windows.

**First Windows build checklist:**

1. **Default build still clean:** `cmake -S . -B build-default && cmake --build build-default --config Release --target z_generals z_generalszh`. No `GGC_RENDER_BACKEND` flag set — defaults to `dx8`. Should produce a byte-identical binary to `superhackers/main@df31c7eae` + the Phase 1 changes. No bgfx/Diligent is fetched.

2. **bgfx build succeeds:** `cmake -S . -B build-bgfx -DGGC_RENDER_BACKEND=bgfx && cmake --build build-bgfx --config Release --target z_generalszh`. Expected behavior:
   - CMake configure: FetchContent clones `bgfx.cmake` + nested `bgfx`/`bx`/`bimg`, runs their CMakeLists, produces targets `bgfx`/`bx`/`bimg`/`shaderc`.
   - Build: `BgfxBackend.cpp` compiles (requires bgfx headers to be reachable via the propagated include directories). `bgfx`/`bx`/`bimg` static libs compile. `generalszh.exe` links.
   - Runtime: launching shows the stderr message `"[BgfxBackend] Phase 2 stub backend constructed..."`, then the game boots into DX8-driven rendering (because only `W3DStatusCircle` is migrated and its calls silently no-op). No crash expected.

3. **Diligent build succeeds:** same pattern with `-DGGC_RENDER_BACKEND=diligent`.

**Likely compile failures on first Windows build and fixes:**

- **"bgfx header not reachable from BgfxBackend.cpp"** — the `target_link_libraries(corei_ww3d2 INTERFACE bgfx bx bimg)` line should propagate include directories too. If it doesn't, add `target_include_directories(corei_ww3d2 INTERFACE $<TARGET_PROPERTY:bgfx,INTERFACE_INCLUDE_DIRECTORIES>)` or link via `PUBLIC` instead of `INTERFACE`. Check bgfx.cmake's target definition to see what INTERFACE props it sets.
- **"Diligent platform header not found"** — the `#define PLATFORM_WIN32 1` in `DiligentBackend.cpp` may not be enough; Diligent's CMake might set platform defines as target properties that need to propagate. Fix by defining them in the CMakeLists.txt via `target_compile_definitions(corei_ww3d2 INTERFACE PLATFORM_WIN32=1)` when `GGC_RENDER_BACKEND=diligent`.
- **"`[[maybe_unused]]` not supported"** — I used C++17 attributes. If the build compiler is older, fall back to `(void)kBgfxLinkAnchor;` in an init block.
- **Linker can't find `bgfx::getCaps`** — the link anchor approach assumes `bgfx::getCaps` is exported. If bgfx uses internal linkage for it in static builds, pick a different symbol (e.g. `bgfx::Init` constructor or `bgfx::getInternalData`).

None of these are hard to fix — each is a 1-5 line tweak.

## Phase 3 preview

Phase 3 migrates the remaining rendering subsystems from `DX8Wrapper::` statics to `g_renderBackend->` calls, fills in the corresponding stubs in `BgfxBackend` and `DiligentBackend`, and unlocks real rendering through non-DX8 backends one subsystem at a time. Sketch of the order (copied from RENDER_BACKEND.md):

1. 2D UI / `render2d` — isolated, simplest
2. Debug geometry
3. Static W3D meshes via `DX8TextureCategoryClass`
4. Heat haze (`W3DSmudge`)
5. Terrain
6. Shadows
7. Water
8. Particles

Each Phase 3 subsystem migration produces a three-part commit:
- Migrate the subsystem's `DX8Wrapper::*` calls to `g_renderBackend->*`
- Fill in the corresponding stubs in `BgfxBackend.cpp` with real bgfx calls
- Fill in the same stubs in `DiligentBackend.cpp` with real Diligent calls

(If maintaining both backends becomes too much work, Phase 3 can pick one as the primary and leave the other as stubs until it catches up. The `GGC_RENDER_BACKEND` flag lets both coexist without interfering with each other.)

## Phase 4+ preview

Phase 4 adds Linux and macOS native build targets once enough subsystems are migrated that the game actually renders through a non-DX8 backend. bgfx and Diligent are both cross-platform-ready; the work is mostly in the rest of WW3D2 (texture loading, device creation, file I/O) which still depends on DX8/Windows specifically.

Phase 5 decides whether to retire the DX8 path or keep it permanently as a reference implementation.


## Exit criterion

`cmake -S . -B build -DGGC_RENDER_BACKEND=bgfx` clones bgfx.cmake via FetchContent, builds the bgfx static libs, builds `z_ww3d2` with `BgfxBackend.cpp` included, links `generalszh.exe`. Launching it shows a cleared window (dark background) because `Clear()` is real but all the rendering stubs are no-ops. The window responds to messages, doesn't crash, and can be closed.

Same for `-DGGC_RENDER_BACKEND=diligent`.

Default build (`dx8`) is byte-identical to Phase 1 output.
