# cmake/bgfx.cmake
#
# TheSuperHackers @refactor bobtista 10/04/2026 bgfx dependency for the
# GGC_RENDER_BACKEND=bgfx build. Included from cmake/render-backend.cmake.
#
# Pulls in bgfx via the community bgfx.cmake wrapper, which internally
# fetches bgfx/bx/bimg as git submodules. We pin a specific bgfx.cmake
# SHA for reproducibility. See PHASE2.md.
#
# This file is NOT included when GGC_RENDER_BACKEND is dx8 or diligent.

# Disable bgfx features we don't need. These must be set BEFORE
# FetchContent_MakeAvailable so bgfx.cmake picks them up at configure time.
set(BGFX_BUILD_EXAMPLES       OFF CACHE BOOL "" FORCE)
set(BGFX_BUILD_TESTS          OFF CACHE BOOL "" FORCE)
set(BGFX_BUILD_TOOLS          ON  CACHE BOOL "" FORCE)  # shaderc is mandatory
set(BGFX_BUILD_TOOLS_BIN2C    ON  CACHE BOOL "" FORCE)
set(BGFX_BUILD_TOOLS_SHADER   ON  CACHE BOOL "" FORCE)
set(BGFX_BUILD_TOOLS_GEOMETRY OFF CACHE BOOL "" FORCE)
set(BGFX_BUILD_TOOLS_TEXTURE  OFF CACHE BOOL "" FORCE)
set(BGFX_INSTALL              OFF CACHE BOOL "" FORCE)
set(BGFX_CUSTOM_TARGETS       OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    bgfx_cmake
    GIT_REPOSITORY https://github.com/bkaradzic/bgfx.cmake.git
    GIT_TAG        668550dc7c47c71860a39c5ef4c162e79294c93f
    # Nested submodules (bgfx, bx, bimg) are cloned recursively by FetchContent.
    GIT_SUBMODULES_RECURSE TRUE
)

FetchContent_MakeAvailable(bgfx_cmake)

# IDE organization.
foreach(_t bgfx bx bimg shaderc bimg_decode bimg_encode)
    if(TARGET ${_t})
        set_target_properties(${_t} PROPERTIES FOLDER "Dependencies/bgfx")
    endif()
endforeach()
