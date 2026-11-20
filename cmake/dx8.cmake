if(GGC_RENDER_BACKEND STREQUAL "bgfx")
    add_library(d3d8lib INTERFACE)
    target_include_directories(d3d8lib INTERFACE
        ${CMAKE_SOURCE_DIR}/Core/Libraries/Source/WWVegas/WW3D2/dx8sdk)
    target_compile_definitions(d3d8lib INTERFACE -DBUILD_WITH_D3D8)
    if(WIN32 OR "${CMAKE_SYSTEM}" MATCHES "Windows")
        target_link_libraries(d3d8lib INTERFACE dinput8 dxguid)
    endif()

    # TheSuperHackers @build bobtista 18/11/2026 BezierSegment includes d3dx8math.h on Windows and
    # calls D3DXVec4Transform from d3dx8.lib. Keep that library in the 32-bit MSVC bgfx build so the
    # simulation matches the dx8 build.
    if(MSVC AND CMAKE_SIZEOF_VOID_P EQUAL 4)
        FetchContent_Declare(
            dx8
            GIT_REPOSITORY https://github.com/TheSuperHackers/min-dx8-sdk.git
            GIT_TAG        7bddff8c01f5fb931c3cb73d4aa8e66d303d97bc
            SOURCE_SUBDIR  no-cmake-project
        )

        FetchContent_MakeAvailable(dx8)

        add_library(d3dx8math_sdk INTERFACE)
        target_include_directories(d3dx8math_sdk INTERFACE ${dx8_SOURCE_DIR})
        target_link_directories(d3dx8math_sdk INTERFACE ${dx8_SOURCE_DIR})
        target_link_libraries(d3dx8math_sdk INTERFACE d3dx8 legacy_stdio_definitions)
        target_link_options(d3dx8math_sdk INTERFACE /NODEFAULTLIB:libci.lib /SAFESEH:NO)
    endif()
else()
    FetchContent_Declare(
        dx8
        GIT_REPOSITORY https://github.com/TheSuperHackers/min-dx8-sdk.git
        GIT_TAG        7bddff8c01f5fb931c3cb73d4aa8e66d303d97bc
    )

    FetchContent_MakeAvailable(dx8)
endif()
