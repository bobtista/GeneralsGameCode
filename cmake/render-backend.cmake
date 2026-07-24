# cmake/render-backend.cmake
#
# TheSuperHackers @refactor bobtista 10/04/2026 Selects the rendering backend
# for WW3D2 at configure time.
#
# Valid values:
#   dx8  - existing DirectX 8 backend. Default. VC6-compatible. Windows only.
#   bgfx - bgfx abstraction over DX11/Vulkan/Metal/GL. Cross-platform. MSVC 2022+.
#
# When set to bgfx the dependency module is included from cmake/bgfx.cmake.
# It is not fetched when dx8 is selected.
#
# This file must be included from the top-level CMakeLists.txt after the
# project() call but before the WW3D2 source subdirectories are added.

set(GGC_RENDER_BACKEND "dx8" CACHE STRING
    "Rendering backend for WW3D2: dx8 (default) or bgfx")
set_property(CACHE GGC_RENDER_BACKEND PROPERTY STRINGS dx8 bgfx)

if(NOT GGC_RENDER_BACKEND STREQUAL "dx8" AND
   NOT GGC_RENDER_BACKEND STREQUAL "bgfx")
    message(FATAL_ERROR
        "Invalid GGC_RENDER_BACKEND: '${GGC_RENDER_BACKEND}'. "
        "Must be one of: dx8, bgfx.")
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
            "Cross-platform support is still landing; expect compile failures outside Windows.")
    endif()
endif()

# Expose the selection as a compile definition so downstream code can do
#   #if defined(GGC_RENDER_BACKEND_BGFX)
# without coupling to the raw string variable.
if(GGC_RENDER_BACKEND STREQUAL "dx8")
    set(GGC_RENDER_BACKEND_COMPILE_DEFINE "GGC_RENDER_BACKEND_DX8=1")
elseif(GGC_RENDER_BACKEND STREQUAL "bgfx")
    set(GGC_RENDER_BACKEND_COMPILE_DEFINE "GGC_RENDER_BACKEND_BGFX=1")
endif()

# TheSuperHackers @refactor bobtista 03/06/2026 The bgfx backend no longer
# creates a real D3D8 device or a DX8 reference popup; it always renders to the
# main window directly. The former GGC_BGFX_STANDALONE toggle is gone - the
# GGC_RENDER_BACKEND_BGFX compile define now drives every former-standalone
# code path, and the dx8/d3d8 runtime is linked only for GGC_RENDER_BACKEND=dx8.

if(GGC_RENDER_BACKEND STREQUAL "bgfx")
    if(NOT DEFINED GGC_BGFX_RENDERER)
        if(APPLE)
            set(GGC_BGFX_RENDERER "metal" CACHE STRING "bgfx renderer for GGC_RENDER_BACKEND=bgfx")
        elseif(UNIX)
            set(GGC_BGFX_RENDERER "vulkan" CACHE STRING "bgfx renderer for GGC_RENDER_BACKEND=bgfx")
        else()
            set(GGC_BGFX_RENDERER "dx11" CACHE STRING "bgfx renderer for GGC_RENDER_BACKEND=bgfx")
        endif()
    endif()
    set_property(CACHE GGC_BGFX_RENDERER PROPERTY STRINGS dx11 metal vulkan)

    if(NOT GGC_BGFX_RENDERER STREQUAL "dx11" AND
       NOT GGC_BGFX_RENDERER STREQUAL "metal" AND
       NOT GGC_BGFX_RENDERER STREQUAL "vulkan")
        message(FATAL_ERROR
            "Invalid GGC_BGFX_RENDERER: '${GGC_BGFX_RENDERER}'. "
            "Must be one of: dx11, metal, vulkan.")
    endif()

    if(GGC_BGFX_RENDERER STREQUAL "metal")
        add_compile_definitions(GGC_BGFX_RENDERER_METAL=1)
    elseif(GGC_BGFX_RENDERER STREQUAL "vulkan")
        add_compile_definitions(GGC_BGFX_RENDERER_VULKAN=1)
    else()
        add_compile_definitions(GGC_BGFX_RENDERER_DX11=1)
    endif()
    message(STATUS "bgfx renderer target: ${GGC_BGFX_RENDERER}")
endif()

# Pull in the backend's dependency module if it has one. dx8 has no module
# here because cmake/dx8.cmake is already included from the top-level
# CMakeLists.txt unconditionally for the min-dx8-sdk.
if(GGC_RENDER_BACKEND STREQUAL "bgfx")
    include(cmake/bgfx.cmake)
endif()
