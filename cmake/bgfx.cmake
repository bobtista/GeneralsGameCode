# cmake/bgfx.cmake
#
# TheSuperHackers @refactor bobtista 10/04/2026 bgfx dependency for the
# GGC_RENDER_BACKEND=bgfx build. Included from cmake/render-backend.cmake.
#
# Pulls in bgfx via the community bgfx.cmake wrapper, which internally
# fetches bgfx/bx/bimg as git submodules. We pin a specific bgfx.cmake
# SHA for reproducibility.
#
# This file is NOT included when GGC_RENDER_BACKEND is dx8 or diligent.

# Disable bgfx features we don't need. These must be set BEFORE
# FetchContent_MakeAvailable so bgfx.cmake picks them up at configure time.
set(BGFX_BUILD_EXAMPLES       OFF CACHE BOOL "" FORCE)
set(BGFX_BUILD_TESTS          OFF CACHE BOOL "" FORCE)
set(BGFX_BUILD_TOOLS          ON  CACHE BOOL "" FORCE)  # shaderc is mandatory
set(BGFX_BUILD_TOOLS_BIN2C    ON  CACHE BOOL "" FORCE)
set(BGFX_BUILD_TOOLS_SHADER   ON  CACHE BOOL "" FORCE)
set(BGFX_BUILD_TOOLS_GEOMETRY OFF CACHE BOOL "" FORCE)
set(BGFX_BUILD_TOOLS_TEXTURE  OFF CACHE BOOL "" FORCE)
set(BGFX_INSTALL              OFF CACHE BOOL "" FORCE)
set(BGFX_CUSTOM_TARGETS       OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    bgfx_cmake
    GIT_REPOSITORY https://github.com/bkaradzic/bgfx.cmake.git
    GIT_TAG        668550dc7c47c71860a39c5ef4c162e79294c93f
    # Nested submodules (bgfx, bx, bimg) are cloned recursively by FetchContent.
    GIT_SUBMODULES_RECURSE TRUE
)

FetchContent_MakeAvailable(bgfx_cmake)

# IDE organization.
foreach(_t bgfx bx bimg shaderc bimg_decode bimg_encode)
    if(TARGET ${_t})
        set_target_properties(${_t} PROPERTIES FOLDER "Dependencies/bgfx")
    endif()
endforeach()

# TheSuperHackers @refactor bobtista 11/04/2026 Phase 4B.1 shader compilation.
# bgfx shaders are authored in ".sc" files (GLSL-ish with bgfx pragmas) and
# compiled by the shaderc tool (built as part of bgfx.cmake) into per-platform
# bytecode. The --bin2c option emits a C header with the compiled bytecode as
# a uint8_t array, which we then #include from BgfxBackend.cpp and hand to
# bgfx::createShader via bgfx::makeRef.
#
# For Phase 4 we only target Direct3D 11 on Windows (profile s_5_0). Later
# phases can add GLSL / SPIR-V / Metal variants if we need non-Windows hosts.
#
# Usage in callers:
#   ggc_compile_bgfx_shader(<source.sc>)   # once per shader file
# Then link the ggc_bgfx_shaders target into whatever library consumes the
# generated headers (from either the corei_ww3d2 INTERFACE chain or directly).
#
# The generated header ends up at ${CMAKE_BINARY_DIR}/ggc_bgfx_shaders/
# with the C array named from the basename, e.g. vs_passthrough_dx11.
#
# The varying.def.sc file is resolved relative to the .sc file's directory
# and must exist there.
#
# The ggc_bgfx_shaders target is a STATIC library with a single dummy .cpp so
# the generated header files have a concrete library to live on. Making it a
# real library (not INTERFACE) is deliberate: CMake's INCLUDE_DIRECTORIES
# propagation through INTERFACE libraries was flaky for our use case and the
# dependency from consumers to the custom_command outputs wasn't firing
# reliably. STATIC + PUBLIC include dirs + link chain is the robust path.

# Shaderc include path (where bgfx_shader.sh lives in the fetched bgfx tree).
set(GGC_BGFX_SHADER_INCLUDE_DIR "${bgfx_cmake_SOURCE_DIR}/bgfx/src" CACHE INTERNAL "")

# Shared output directory for every compiled shader header.
set(GGC_BGFX_SHADERS_OUT_DIR "${CMAKE_BINARY_DIR}/ggc_bgfx_shaders" CACHE INTERNAL "")

# One-time setup: create the ggc_bgfx_shaders target with a dummy source so
# it compiles as a real STATIC library.
function(ggc_bgfx_shaders_init)
    if(TARGET ggc_bgfx_shaders)
        return()
    endif()

    file(MAKE_DIRECTORY "${GGC_BGFX_SHADERS_OUT_DIR}")
    set(_dummy "${GGC_BGFX_SHADERS_OUT_DIR}/_ggc_bgfx_shaders_dummy.cpp")
    if(NOT EXISTS "${_dummy}")
        file(WRITE "${_dummy}"
             "// Auto-generated. Exists only so ggc_bgfx_shaders has a compilable source.\n"
             "namespace { char ggc_bgfx_shaders_anchor = 0; }\n")
    endif()

    add_library(ggc_bgfx_shaders STATIC "${_dummy}")
    set_target_properties(ggc_bgfx_shaders PROPERTIES FOLDER "Dependencies/bgfx")
    target_include_directories(ggc_bgfx_shaders PUBLIC "${GGC_BGFX_SHADERS_OUT_DIR}")
endfunction()

function(ggc_compile_bgfx_shader source_sc)
    if(NOT TARGET shaderc)
        message(FATAL_ERROR "ggc_compile_bgfx_shader: shaderc target not available. "
                            "Ensure BGFX_BUILD_TOOLS_SHADER=ON and bgfx.cmake is included.")
    endif()

    ggc_bgfx_shaders_init()

    get_filename_component(_sc_abs "${source_sc}" ABSOLUTE)
    get_filename_component(_sc_dir "${_sc_abs}" DIRECTORY)
    get_filename_component(_sc_name "${_sc_abs}" NAME_WE)

    if(_sc_name MATCHES "^vs_")
        set(_shader_type "vertex")
    elseif(_sc_name MATCHES "^fs_")
        set(_shader_type "fragment")
    else()
        message(FATAL_ERROR "ggc_compile_bgfx_shader: '${_sc_name}' must start with vs_ or fs_.")
    endif()

    set(_out_header "${GGC_BGFX_SHADERS_OUT_DIR}/${_sc_name}_dx11.bin.h")
    set(_varname "${_sc_name}_dx11")
    set(_varying_def "${_sc_dir}/varying.def.sc")

    add_custom_command(
        OUTPUT "${_out_header}"
        COMMAND "$<TARGET_FILE:shaderc>"
            -f "${_sc_abs}"
            -o "${_out_header}"
            --bin2c "${_varname}"
            -i "${GGC_BGFX_SHADER_INCLUDE_DIR}"
            --platform windows
            --profile s_5_0
            --type "${_shader_type}"
            --varyingdef "${_varying_def}"
            -O 3
        MAIN_DEPENDENCY "${_sc_abs}"
        DEPENDS "${_varying_def}" shaderc
        COMMENT "Compiling bgfx shader ${_sc_name}"
        VERBATIM
    )

    target_sources(ggc_bgfx_shaders PRIVATE "${_out_header}")
    set_source_files_properties("${_out_header}" PROPERTIES
        GENERATED TRUE
        HEADER_FILE_ONLY TRUE
    )
endfunction()
