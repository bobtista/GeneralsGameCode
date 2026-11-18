set(PROFILE_SRC
    "Include/profile/profile_doc.h"
    "Include/profile/profile_funclevel.h"
    "Include/profile/profile_highlevel.h"
    "Include/profile/profile_result.h"
    "Include/profile/profile.h"
    "internal_cmd.h"
    "internal_funclevel.h"
    "internal_highlevel.h"
    "internal_result.h"
    "internal.h"
    "profile_cmd.cpp"
    "profile_funclevel.cpp"
    "profile_highlevel.cpp"
    "profile_result.cpp"
    "profile.cpp"
)

add_library(core_profile_legacy STATIC)

target_sources(core_profile_legacy PRIVATE ${PROFILE_SRC})

target_include_directories(core_profile_legacy PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/Include
)

if(NOT WIN32)
    target_include_directories(core_profile_legacy BEFORE PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../WWVegas/compat/win32_shims
        ${CMAKE_CURRENT_SOURCE_DIR}/../WWVegas
        ${CMAKE_CURRENT_SOURCE_DIR}/../debug/Include
    )
endif()

# TheSuperHackers @build bobtista 29/04/2026 Pull windows.h in via the compat
# shim before profile.h on non-Win so __int64 / __forceinline are defined.
if(WIN32)
    target_precompile_headers(core_profile_legacy PRIVATE
        [["Precompiled/precompiled.h"]] # Must be first
        "Include/profile/profile.h"
        "internal.h"
        <windows.h>
    )
else()
    target_precompile_headers(core_profile_legacy PRIVATE
        [["Precompiled/precompiled.h"]] # Must be first
        <windows.h>
        "Include/profile/profile.h"
        "internal.h"
    )
endif()

target_link_libraries(core_profile_legacy PRIVATE
    depsi_always # Gives force included headers. Link early.
    core_config
    deps_utility
    stlport
)

# TheSuperHackers @build bobtista 24/08/2026 core_debug is Windows-only, so this
# consumer has to guard the link (see Core/Libraries/Source/debug/CMakeLists.txt).
if(WIN32)
    target_link_libraries(core_profile_legacy PRIVATE
        core_debug
    )
endif()
