# Interface libraries to set common defines and includes, avoid duplication later
add_library(core_wwcommon INTERFACE)

target_compile_definitions(core_wwcommon INTERFACE
    #NOMINMAX
    WIN32_LEAN_AND_MEAN
)

target_link_libraries(core_wwcommon INTERFACE
    stlport
)

if(NOT GGC_BGFX_STANDALONE)
    target_link_libraries(core_wwcommon INTERFACE
        d3d8lib
    )
endif()

# TheSuperHackers @build bobtista 29/04/2026 milesstub is Win-only (the
# Miles Sound System SDK stub). On non-Win we use OpenAL instead.
if(WIN32)
    target_link_libraries(core_wwcommon INTERFACE
        milesstub
    )
endif()

target_include_directories(core_wwcommon INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
    WW3D2
    WWAudio
    WWDebug
    # WWDownload # is not added because headers are included as "WWDownload/file.h"
    WWLib
    WWMath
    WWSaveLoad
)

if(NOT WIN32)
    # TheSuperHackers @build bobtista 29/04/2026 BEFORE so the compat shims
    # (e.g. dinput.h) take priority over headers that come along for the ride
    # via FetchContent (the dx8 SDK ships a Win-flavored dinput.h that fails
    # to parse on macOS/Linux).
    target_include_directories(core_wwcommon BEFORE INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/compat/win32_shims
    )
endif()

add_subdirectory(WW3D2)
add_subdirectory(WWAudio)
add_subdirectory(WWDebug)
add_subdirectory(WWDownload)
add_subdirectory(WWLib)
add_subdirectory(WWMath)
add_subdirectory(WWSaveLoad)
add_subdirectory(WWStub)

# Helpful interface to bundle the ww modules together.
add_library(core_wwvegas INTERFACE)

target_include_directories(core_wwvegas INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
    WW3D2
    WWAudio
    WWDebug
    # WWDownload # is not added because headers are included as "WWDownload/file.h"
    WWLib
    WWMath
    WWSaveLoad
)

# TheSuperHackers @build bobtista 29/04/2026 Propagate the win32 shim include
# path through core_wwvegas so consumers (z_gameengine, etc.) can resolve
# <windows.h> and friends to the compat layer without each linking core_wwcommon
# transitively. core_wwlib only links core_wwcommon PRIVATE.
if(NOT WIN32)
    target_include_directories(core_wwvegas BEFORE INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/compat/win32_shims
    )
endif()

target_link_libraries(core_wwvegas INTERFACE
    # core_ww3d2
    # core_wwaudio
    core_wwdebug
    # core_wwdownload
    core_wwlib
    core_wwmath
    core_wwsaveload
)
