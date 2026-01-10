# TheSuperHackers @feature Semantic JSON converter for SCB script files

# Skip on VC6 builds (JSON library requires C++11 or later)
if(IS_VS6_BUILD)
    return()
endif()

add_executable(z_semanticscriptconverter)
set_target_properties(z_semanticscriptconverter PROPERTIES OUTPUT_NAME semanticscriptconverter WIN32_EXECUTABLE FALSE)

target_compile_definitions(z_semanticscriptconverter PRIVATE
    RTS_HAS_JSON_CHUNK
)

target_link_libraries(z_semanticscriptconverter PRIVATE
    corei_semanticscriptconverter
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
