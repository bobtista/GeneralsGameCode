# TheSuperHackers @refactor bobtista 01/01/2025 Migrated from MFC to Qt

set(DEBUGWINDOW_SRC
    "DebugWindow.cpp"
    "DebugWindow.h"
    "DebugWindowDialog.cpp"
    "DebugWindowDialog.h"
    "DebugWindowDialog.ui"
    "DebugWindowExport.h"
)

add_library(core_debugwindow SHARED)

target_sources(core_debugwindow PRIVATE ${DEBUGWINDOW_SRC})

target_link_libraries(core_debugwindow PRIVATE
    core_config
    core_qt
)

if(WIN32 OR "${CMAKE_SYSTEM}" MATCHES "Windows")
    target_sources(core_debugwindow PRIVATE
        DebugWindow.def
    )
    set_target_properties(core_debugwindow PROPERTIES OUTPUT_NAME DebugWindow)
else()
    set_target_properties(core_debugwindow PROPERTIES OUTPUT_NAME debugwindow)
    # On Unix/Mac, export all symbols by default for shared library
    set_target_properties(core_debugwindow PROPERTIES
        CXX_VISIBILITY_PRESET default
        VISIBILITY_INLINES_HIDDEN OFF
    )
endif()
