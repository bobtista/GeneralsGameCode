add_library(core_config INTERFACE)
if(NOT MSVC)
    target_compile_options(core_config INTERFACE "SHELL:-include ${CMAKE_SOURCE_DIR}/Core/Libraries/Include/Lib/MsvcX86Math.h")
endif()

include(${CMAKE_CURRENT_LIST_DIR}/config-build.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/config-debug.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/config-memory.cmake)
