set(PROFILE_SRC
    "internal_cmd.h"
    "internal_funclevel.h"
    "internal_highlevel.h"
    "internal_result.h"
    "internal.h"
    "profile_cmd.cpp"
    "profile_doc.h"
    "profile_funclevel.cpp"
    "profile_funclevel.h"
    "profile_highlevel.cpp"
    "profile_highlevel.h"
    "profile_result.cpp"
    "profile_result.h"
    "profile.cpp"
    "profile.h"
)

add_library(core_profile_legacy STATIC)

target_sources(core_profile_legacy PRIVATE ${PROFILE_SRC})

target_include_directories(core_profile_legacy INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# TheSuperHackers @build bobtista 29/04/2026 Pull windows.h in via the compat
# shim before profile.h on non-Win so __int64 / __forceinline are defined.
if(WIN32)
    target_precompile_headers(core_profile_legacy PRIVATE
        [["Utility/CppMacros.h"]] # Must be first, to be removed when abandoning VC6
        "profile.h"
        "internal.h"
        <windows.h>
    )
else()
    target_precompile_headers(core_profile_legacy PRIVATE
        [["Utility/CppMacros.h"]] # Must be first, to be removed when abandoning VC6
        <windows.h>
        "profile.h"
        "internal.h"
    )
endif()

target_link_libraries(core_profile_legacy PRIVATE
    core_wwcommon
    corei_always
)
