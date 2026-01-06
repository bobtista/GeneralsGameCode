# VC6 is too old to compile nlohmann_json
if(IS_VS6_BUILD)
    return()
endif()

# Try vcpkg first, fallback to FetchContent
find_package(nlohmann_json CONFIG QUIET)
if(NOT nlohmann_json_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3
    )
    FetchContent_MakeAvailable(nlohmann_json)
endif()

add_executable(core_datachunkconverter)
set_target_properties(core_datachunkconverter PROPERTIES OUTPUT_NAME DataChunkConverter)
target_compile_features(core_datachunkconverter PRIVATE cxx_std_17)

target_sources(core_datachunkconverter PRIVATE
    DataChunkConverter.cpp
)

target_link_libraries(core_datachunkconverter PRIVATE
    nlohmann_json::nlohmann_json
)

if(WIN32 OR "${CMAKE_SYSTEM}" MATCHES "Windows")
    target_link_options(core_datachunkconverter PRIVATE /subsystem:console)
endif()
