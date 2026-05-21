FetchContent_Declare(
    dx8
    GIT_REPOSITORY https://github.com/TheSuperHackers/min-dx8-sdk.git
    GIT_TAG        7bddff8c01f5fb931c3cb73d4aa8e66d303d97bc
)

FetchContent_MakeAvailable(dx8)

# TheSuperHackers @build bobtista 22/04/2026 Phase 5.2 Stage 5
# Standalone bgfx still needs the min-DX8 headers for legacy type names, but it
# must not link the real d3d8.lib / d3dx8.lib. D3D device ownership lives in
# bgfx, and the remaining D3DX entry points are supplied by
# D3DXStandaloneStubs.cpp until those callers are migrated.
if(GGC_BGFX_STANDALONE)
    set_property(TARGET d3d8lib PROPERTY INTERFACE_LINK_LIBRARIES "")
    if(WIN32 OR "${CMAKE_SYSTEM}" MATCHES "Windows")
        target_link_libraries(d3d8lib INTERFACE dinput8 dxguid)
    endif()
endif()
