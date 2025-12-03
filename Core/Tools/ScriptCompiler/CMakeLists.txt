set(SCRIPTCOMPILER_SRC
    "ScriptCompiler.cpp"
)

add_executable(core_scriptcompiler)
set_target_properties(core_scriptcompiler PROPERTIES OUTPUT_NAME scriptcompiler)

target_sources(core_scriptcompiler PRIVATE ${SCRIPTCOMPILER_SRC})

# Try to find nlohmann-json from vcpkg first
find_package(nlohmann_json CONFIG)
if(nlohmann_json_FOUND)
    # Use vcpkg version
    target_link_libraries(core_scriptcompiler PRIVATE nlohmann_json::nlohmann_json)
else()
    # Fallback: fetch from GitHub (header-only library)
    include(FetchContent)
    FetchContent_Declare(json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(json)
    target_link_libraries(core_scriptcompiler PRIVATE nlohmann_json::nlohmann_json)
endif()

target_link_libraries(core_scriptcompiler PRIVATE
    core_datachunk
)

if(WIN32 OR "${CMAKE_SYSTEM}" MATCHES "Windows")
    target_link_options(core_scriptcompiler PRIVATE /subsystem:console)
endif()

