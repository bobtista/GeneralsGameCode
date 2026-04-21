# cmake/render-backend.cmake
#
# TheSuperHackers @refactor bobtista 10/04/2026 Selects the rendering backend
# for WW3D2 at configure time. See Core/Libraries/Source/WWVegas/WW3D2/RENDER_BACKEND.md
# for the full multi-phase plan and PHASE2.md for this phase's scope.
#
# Valid values:
#   dx8      - existing DirectX 8 backend. Default. VC6-compatible. Windows only.
#   bgfx     - bgfx abstraction over DX11/Vulkan/Metal/GL. Cross-platform. MSVC 2022+.
#   diligent - Diligent Engine abstraction over DX11/Vulkan/Metal. Cross-platform. MSVC 2022+.
#
# When set to bgfx or diligent, the corresponding dependency module is
# included from cmake/bgfx.cmake or cmake/diligent.cmake. Neither is
# fetched when dx8 is selected.
#
# This file must be included from the top-level CMakeLists.txt after the
# project() call but before the WW3D2 source subdirectories are added.

set(GGC_RENDER_BACKEND "dx8" CACHE STRING
    "Rendering backend for WW3D2: dx8 (default), bgfx, or diligent")
set_property(CACHE GGC_RENDER_BACKEND PROPERTY STRINGS dx8 bgfx diligent)

if(NOT GGC_RENDER_BACKEND STREQUAL "dx8" AND
   NOT GGC_RENDER_BACKEND STREQUAL "bgfx" AND
   NOT GGC_RENDER_BACKEND STREQUAL "diligent")
    message(FATAL_ERROR
        "Invalid GGC_RENDER_BACKEND: '${GGC_RENDER_BACKEND}'. "
        "Must be one of: dx8, bgfx, diligent.")
endif()

message(STATUS "WW3D2 render backend: ${GGC_RENDER_BACKEND}")

# Non-DX8 backends require a modern toolchain. We do NOT want to silently
# pick a broken build configuration, so fail early with a useful message.
if(NOT GGC_RENDER_BACKEND STREQUAL "dx8")
    if(IS_VS6_BUILD)
        message(FATAL_ERROR
            "GGC_RENDER_BACKEND=${GGC_RENDER_BACKEND} requires a modern C++ toolchain "
            "(MSVC 2022 or Clang 11+). VC6 builds must use GGC_RENDER_BACKEND=dx8.")
    endif()
    if(NOT WIN32)
        message(WARNING
            "GGC_RENDER_BACKEND=${GGC_RENDER_BACKEND} is being configured on a non-Windows host. "
            "Cross-platform build targets will land in Phase 4; for now expect compile failures "
            "outside Windows.")
    endif()
endif()

# Expose the selection as a compile definition so downstream code can do
#   #if defined(GGC_RENDER_BACKEND_BGFX)
# without coupling to the raw string variable.
if(GGC_RENDER_BACKEND STREQUAL "dx8")
    set(GGC_RENDER_BACKEND_COMPILE_DEFINE "GGC_RENDER_BACKEND_DX8=1")
elseif(GGC_RENDER_BACKEND STREQUAL "bgfx")
    set(GGC_RENDER_BACKEND_COMPILE_DEFINE "GGC_RENDER_BACKEND_BGFX=1")
elseif(GGC_RENDER_BACKEND STREQUAL "diligent")
    set(GGC_RENDER_BACKEND_COMPILE_DEFINE "GGC_RENDER_BACKEND_DILIGENT=1")
endif()

# Pull in the backend's dependency module if it has one. dx8 has no module
# here because cmake/dx8.cmake is already included from the top-level
# CMakeLists.txt unconditionally for the min-dx8-sdk.
if(GGC_RENDER_BACKEND STREQUAL "bgfx")
    include(cmake/bgfx.cmake)
elseif(GGC_RENDER_BACKEND STREQUAL "diligent")
    include(cmake/diligent.cmake)
endif()
