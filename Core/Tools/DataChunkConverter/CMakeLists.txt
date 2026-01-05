find_package(nlohmann_json CONFIG REQUIRED)

add_executable(core_datachunkconverter)
set_target_properties(core_datachunkconverter PROPERTIES OUTPUT_NAME DataChunkConverter)

target_sources(core_datachunkconverter PRIVATE
    DataChunkConverter.cpp
)

target_link_libraries(core_datachunkconverter PRIVATE
    nlohmann_json::nlohmann_json
)

if(WIN32 OR "${CMAKE_SYSTEM}" MATCHES "Windows")
    target_link_options(core_datachunkconverter PRIVATE /subsystem:console)
endif()
