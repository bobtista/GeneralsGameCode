if(GGC_RENDER_BACKEND STREQUAL "bgfx")
    add_library(d3d8lib INTERFACE)
    target_include_directories(d3d8lib INTERFACE
        ${CMAKE_SOURCE_DIR}/Core/Libraries/Source/WWVegas/WW3D2/dx8sdk)
    target_compile_definitions(d3d8lib INTERFACE -DBUILD_WITH_D3D8)
    if(WIN32 OR "${CMAKE_SYSTEM}" MATCHES "Windows")
        target_link_libraries(d3d8lib INTERFACE dinput8 dxguid)
    endif()
else()
    FetchContent_Declare(
        dx8
        GIT_REPOSITORY https://github.com/TheSuperHackers/min-dx8-sdk.git
        GIT_TAG        7bddff8c01f5fb931c3cb73d4aa8e66d303d97bc
    )

    FetchContent_MakeAvailable(dx8)
endif()
