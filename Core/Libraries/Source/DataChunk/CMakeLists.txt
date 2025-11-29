set(DATACHUNK_SRC
    "TableOfContents.cpp"
    "DataChunkOutput.cpp"
    "DataChunkInput.cpp"
)

set(DATACHUNK_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../Include/DataChunk")

add_library(core_datachunk STATIC ${DATACHUNK_SRC})

target_include_directories(core_datachunk PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/../../Include"
)

target_link_libraries(core_datachunk PUBLIC
    core_utility
)

target_compile_features(core_datachunk PUBLIC cxx_std_98)

if(WIN32 OR "${CMAKE_SYSTEM}" MATCHES "Windows")
    target_compile_definitions(core_datachunk PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

