# TheSuperHackers @feature JSON converter for SCB script files

# Skip on VC6 builds (JSON library requires C++11 or later)
if(IS_VS6_BUILD)
    return()
endif()

set(SCRIPTCONVERTER_SRC
    "Source/Main.cpp"
)

add_library(corei_scriptconverter INTERFACE)

target_sources(corei_scriptconverter INTERFACE ${SCRIPTCONVERTER_SRC})

target_include_directories(corei_scriptconverter INTERFACE
    Include
)
