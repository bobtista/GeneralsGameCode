# Interface libraries to set common defines and includes, avoid duplication later
add_library(z_wwcommon INTERFACE)

target_link_libraries(z_wwcommon INTERFACE
    core_wwcommon
    stlport
)

if(NOT GGC_RENDER_BACKEND STREQUAL "bgfx")
    target_link_libraries(z_wwcommon INTERFACE
        d3d8lib
    )
endif()

# TheSuperHackers @build bobtista 29/04/2026 milesstub is Win-only.
if(WIN32)
    target_link_libraries(z_wwcommon INTERFACE
        milesstub
    )
endif()

target_include_directories(z_wwcommon INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
    WW3D2
)

add_subdirectory(WWAudio)
add_subdirectory(WW3D2)
add_subdirectory(WWDownload)

# Helpful interface to bundle the ww modules together.
add_library(z_wwvegas INTERFACE)

target_include_directories(z_wwvegas INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
    WW3D2
)

target_link_libraries(z_wwvegas INTERFACE
    core_wwvegas
    z_ww3d2
    z_wwdownload
)
