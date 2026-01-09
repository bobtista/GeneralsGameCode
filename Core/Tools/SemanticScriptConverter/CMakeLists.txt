# TheSuperHackers @feature Semantic JSON converter for SCB script files

# Skip on VC6 builds (JSON library requires C++11 or later)
if(IS_VS6_BUILD)
    return()
endif()

set(SEMANTICSCRIPTCONVERTER_SRC
    "Source/Main.cpp"
)

add_library(corei_semanticscriptconverter INTERFACE)

target_sources(corei_semanticscriptconverter INTERFACE ${SEMANTICSCRIPTCONVERTER_SRC})

target_include_directories(corei_semanticscriptconverter INTERFACE
    Include
)
