set(W3DCONVERT_SRC
    "w3dconvert.cpp"
    "chunkserializer.cpp"
    "chunkserializer.h"
    "xmlwriter.cpp"
    "xmlwriter.h"
    "xmlreader.cpp"
    "xmlreader.h"
)

add_executable(z_w3dconvert)
set_target_properties(z_w3dconvert PROPERTIES OUTPUT_NAME w3dconvert)

target_sources(z_w3dconvert PRIVATE ${W3DCONVERT_SRC})

target_link_libraries(z_w3dconvert PRIVATE
    core_config
    core_utility
    core_wwstub
    z_wwvegas
)

target_include_directories(z_w3dconvert PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

