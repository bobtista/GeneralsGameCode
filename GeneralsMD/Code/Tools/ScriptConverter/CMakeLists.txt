# TheSuperHackers @feature JSON converter for SCB script files

# Skip on VC6 builds (JSON library requires C++11 or later)
if(IS_VS6_BUILD)
    return()
endif()

add_executable(z_scriptconverter)
set_target_properties(z_scriptconverter PROPERTIES OUTPUT_NAME scriptconverter WIN32_EXECUTABLE FALSE)

target_compile_definitions(z_scriptconverter PRIVATE
    RTS_HAS_JSON_CHUNK
)

target_link_libraries(z_scriptconverter PRIVATE
    corei_scriptconverter
    z_gameengine
    z_gameenginedevice
    zi_always
    comctl32
    core_compression
    core_debug
    core_profile
    winmm
    vfw32
    imm32
)
