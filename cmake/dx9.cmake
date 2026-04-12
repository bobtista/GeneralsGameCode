FetchContent_Declare(
    dx9
    GIT_REPOSITORY https://github.com/stephanmeesters/min-dx9-sdk.git
    GIT_TAG        d7c1c587f3bbab03900e8a6367669bb56e30e8b3
)

FetchContent_MakeAvailable(dx9)

add_library(dx9sdk INTERFACE)
target_include_directories(dx9sdk SYSTEM INTERFACE BEFORE "${dx9_SOURCE_DIR}")
target_link_directories(dx9sdk INTERFACE "${dx9_SOURCE_DIR}")
target_link_libraries(dx9sdk INTERFACE d3d9)
