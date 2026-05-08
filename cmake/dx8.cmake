FetchContent_Declare(
    dx8
    GIT_REPOSITORY https://github.com/TheSuperHackers/min-dx8-sdk.git
    GIT_TAG        7bddff8c01f5fb931c3cb73d4aa8e66d303d97bc
)

FetchContent_MakeAvailable(dx8)

# TheSuperHackers @build bobtista 22/04/2026 Phase 5.2 Stage 5 —
# In standalone bgfx mode the game supplies d3d8/d3dx8 functions from
# in-tree stubs (Core/Libraries/Source/WWVegas/WW3D2/StubD3D8Device.cpp
# + D3DXStandaloneStubs.cpp). Pulling the real d3d8.lib / d3dx8.lib
# would cause duplicate-symbol link errors, so strip them off the
# shared d3d8lib INTERFACE target. Headers stay (include dir is still
# on d3d8lib), and import libs that can't be stubbed (dinput8, dxguid)
# stay as well. Ref-popup builds are unaffected.
if(GGC_BGFX_STANDALONE)
    set_property(TARGET d3d8lib PROPERTY INTERFACE_LINK_LIBRARIES "")
    target_link_libraries(d3d8lib INTERFACE dinput8 dxguid)
endif()
