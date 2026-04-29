set(DEBUG_SRC
    "debug_cmd.cpp"
    "debug_cmd.h"
    "debug_debug.cpp"
    "debug_debug.h"
    "debug_doc.h"
    "debug_except.cpp"
    "debug_getdefaultcommands.cpp"
    "debug_internal.cpp"
    "debug_io_con.cpp"
    "debug_io_flat.cpp"
    "debug_io_net.cpp"
    "debug_io_ods.cpp"
    "debug_io.h"
    "debug_macro.h"
    "debug_stack.cpp"
    "debug_stack.h"
    "debug.h"
    "internal_except.h"
    "internal_io.h"
    "internal.h"
)

# TheSuperHackers @build bobtista 29/04/2026 core_debug is Windows-only.
# debug_debug.cpp self-#errors on non-Win/MSVC and the Win API surface
# (SetUnhandledExceptionFilter, ReadFile, MessageBox, ...) does not have a
# meaningful POSIX analog. Consumers that link core_debug must guard the
# link on WIN32. Matches the fbraz3/GeneralsX approach.
if(WIN32)
    add_library(core_debug STATIC)

    target_sources(core_debug PRIVATE ${DEBUG_SRC})

    target_include_directories(core_debug INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
    )

    target_precompile_headers(core_debug PRIVATE
        [["Utility/CppMacros.h"]] # Must be first, to be removed when abandoning VC6
        "debug.h"
        "internal.h"
        "internal_except.h"
        "internal_io.h"
        <windows.h>
    )

    target_link_libraries(core_debug PRIVATE
        core_wwcommon
        corei_always
    )
endif()
