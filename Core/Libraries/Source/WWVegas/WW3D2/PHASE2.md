# Render Backend Phase 2 — Add bgfx and Diligent backends

**Branch:** `bobtista/feat/phase2-render-backends`
**Base:** `bobtista/refactor/render-backend-interface` (Phase 1)
**Status:** in progress

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

- [ ] **2.0** Write this document (you are here)
- [ ] **2.1** Add `cmake/render-backend.cmake` with the `GGC_RENDER_BACKEND` option and validation
- [ ] **2.2** Update `RenderBackend.cpp` to `#if`-select the concrete backend
- [ ] **2.3** Add `cmake/bgfx.cmake` with `FetchContent_Declare` for bgfx.cmake
- [ ] **2.4** Write `BgfxBackend.{h,cpp}` with real lifecycle + stubs elsewhere
- [ ] **2.5** Add `cmake/diligent.cmake` with `FetchContent_Declare` for DiligentCore
- [ ] **2.6** Write `DiligentBackend.{h,cpp}` with real lifecycle + stubs elsewhere
- [ ] **2.7** Update this document with completion state + Phase 3 handoff

## Exit criterion

`cmake -S . -B build -DGGC_RENDER_BACKEND=bgfx` clones bgfx.cmake via FetchContent, builds the bgfx static libs, builds `z_ww3d2` with `BgfxBackend.cpp` included, links `generalszh.exe`. Launching it shows a cleared window (dark background) because `Clear()` is real but all the rendering stubs are no-ops. The window responds to messages, doesn't crash, and can be closed.

Same for `-DGGC_RENDER_BACKEND=diligent`.

Default build (`dx8`) is byte-identical to Phase 1 output.
