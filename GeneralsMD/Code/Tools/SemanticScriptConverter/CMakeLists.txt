# TheSuperHackers @feature Semantic JSON converter for SCB script files

# Skip on VC6 builds (JSON library requires C++11 or later)
if(IS_VS6_BUILD)
    return()
endif()

add_executable(z_semanticscriptconverter)
set_target_properties(z_semanticscriptconverter PROPERTIES OUTPUT_NAME semanticscriptconverter)

target_link_libraries(z_semanticscriptconverter PRIVATE
    corei_semanticscriptconverter
    z_gameengine
    zi_always
)
