# cmake/diligent.cmake
#
# TheSuperHackers @refactor bobtista 10/04/2026 Diligent Engine dependency
# for the GGC_RENDER_BACKEND=diligent build. Included from
# cmake/render-backend.cmake.
#
# Pulls in DiligentCore (the minimum subset; no DiligentTools, DiligentFX,
# or DiligentSamples) via FetchContent, pinned to a specific SHA for
# reproducibility. See PHASE2.md.
#
# This file is NOT included when GGC_RENDER_BACKEND is dx8 or bgfx.

# Disable Diligent backends we don't use. Must be set BEFORE FetchContent_MakeAvailable.
set(DILIGENT_NO_DIRECT3D11        OFF CACHE BOOL "" FORCE)  # this is the one we want
set(DILIGENT_NO_DIRECT3D12        ON  CACHE BOOL "" FORCE)
set(DILIGENT_NO_OPENGL            ON  CACHE BOOL "" FORCE)
set(DILIGENT_NO_VULKAN            ON  CACHE BOOL "" FORCE)
set(DILIGENT_NO_METAL             ON  CACHE BOOL "" FORCE)
set(DILIGENT_NO_WEBGPU            ON  CACHE BOOL "" FORCE)
set(DILIGENT_NO_HLSL              OFF CACHE BOOL "" FORCE)
set(DILIGENT_NO_FORMAT_VALIDATION ON  CACHE BOOL "" FORCE)
set(DILIGENT_NO_ARCHIVER          ON  CACHE BOOL "" FORCE)
set(DILIGENT_NO_SUPER_RESOLUTION  ON  CACHE BOOL "" FORCE)
set(DILIGENT_BUILD_TESTS          OFF CACHE BOOL "" FORCE)
set(DILIGENT_INSTALL_CORE         OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    diligent_core
    GIT_REPOSITORY https://github.com/DiligentGraphics/DiligentCore.git
    GIT_TAG        296ca9a891b781a49643948495efcf31b1339f50
)

FetchContent_MakeAvailable(diligent_core)
