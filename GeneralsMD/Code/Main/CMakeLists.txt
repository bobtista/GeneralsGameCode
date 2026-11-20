if(WIN32)
    add_executable(z_generals WIN32)
else()
    add_executable(z_generals)
endif()

# Use a binary name that doesn't conflict with original game.
if("${CMAKE_SYSTEM}" MATCHES "Windows")
    set_target_properties(z_generals PROPERTIES OUTPUT_NAME "GeneralsOnlineZH${RTS_BUILD_OUTPUT_SUFFIX}")
else()
    set_target_properties(z_generals PROPERTIES OUTPUT_NAME "generalszh${RTS_BUILD_OUTPUT_SUFFIX}")
endif()

target_link_libraries(z_generals PRIVATE
    core_profile_legacy
    z_gameengine
    z_gameenginedevice
    zi_always
)

if(WIN32)
    target_link_libraries(z_generals PRIVATE
        binkstub
        comctl32
        core_debug
        dinput8
        dxguid
        imm32
        milesstub
        vfw32
        winmm
    )
endif()

if(SAGE_USE_SDL3)
    target_link_libraries(z_generals PRIVATE sdl3lib)
    # TheSuperHackers @build bobtista 07/06/2026 SDL3Main.cpp uses a cross-platform int main(),
    # but on Windows the exe is built with the GUI subsystem whose default entry is WinMainCRTStartup
    # (expects WinMain). Point the entry at mainCRTStartup so the CRT calls main() directly while
    # staying in the WINDOWS subsystem (no console window), avoiding an SDL_main/WinMain shim.
    if(WIN32 AND MSVC)
        target_link_options(z_generals PRIVATE "/ENTRY:mainCRTStartup")
    endif()
endif()

# TheSuperHackers @build bobtista 22/04/2026
# Standalone bgfx owns the device through bgfx and supplies the remaining
# D3DX helper symbols in-tree, so neither d3d8.lib nor d3dx8.lib is needed.
# Ref-popup keeps both for the real DX8 reference path.
if(NOT GGC_RENDER_BACKEND STREQUAL "bgfx")
    target_link_libraries(z_generals PRIVATE d3d8 d3dx8)
endif()

# TODO Originally referred to build host and user, replace with git info perhaps?
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/GeneratedVersion.h
"#pragma once

#define VERSION_LOCALBUILDNUM 0
#define VERSION_BUILDUSER \"\"
#define VERSION_BUILDLOC \"\"
"
)

# Based on original binary values for these variables.
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/BuildVersion.h
"#pragma once

#define VERSION_MAJOR 1
#define VERSION_MINOR 4
#define VERSION_BUILDNUM 601
"
)

if(MSVC)
    target_link_options(z_generals PRIVATE "/NODEFAULTLIB:libci.lib")
endif()

# TheSuperHackers @build bobtista 09/08/2026 The staged Linux zip keeps its bundled
# SDL3/OpenAL/FFmpeg in libs/ beside the executable, but the shipped binary's RUNPATH holds
# only build-tree paths, so it found them solely through the LD_LIBRARY_PATH that run.sh
# exports and launching ./generalszh directly failed. BUILD_RPATH appends rather than
# replaces, so deploy-linux-generalsmd.sh keeps reading those build-tree entries to walk the
# transitive closure.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set_target_properties(z_generals PROPERTIES BUILD_RPATH "$ORIGIN/libs")
endif()

target_include_directories(z_generals PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)

# TheSuperHackers @build bobtista 07/06/2026 SDL3 is the entry point whenever SAGE_USE_SDL3 is
# set, including on Windows (win32-generalsmd-sdl3-bgfx). The Win32 WinMain entry is only used for
# the non-SDL3 Windows build. Previously SDL3Main was gated on NOT WIN32, so the Windows SDL3 build
# linked WinMain and left TheSDL3Window (defined in SDL3Main.cpp) unresolved.
target_sources(z_generals PRIVATE
    $<$<BOOL:${WIN32}>:VCRuntimeCheck.cpp>
    $<$<AND:$<BOOL:${WIN32}>,$<NOT:$<BOOL:${SAGE_USE_SDL3}>>>:WinMain.cpp>
    $<$<AND:$<BOOL:${WIN32}>,$<NOT:$<BOOL:${SAGE_USE_SDL3}>>>:WinMain.h>
    $<$<BOOL:${SAGE_USE_SDL3}>:SDL3Main.cpp>
)

# Stamp the file version of the runtime this toolset ships, the minimum VCRuntimeCheck.cpp accepts
if(MSVC AND NOT IS_VS6_BUILD)
    # cl.exe lives in VC/Tools/MSVC/<ver>/bin/Host<arch>/<arch>
    get_filename_component(_go_vc_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
    get_filename_component(_go_vc_dir "${_go_vc_dir}/../../../../../.." ABSOLUTE)
    set(_go_redist_version_file "${_go_vc_dir}/Auxiliary/Build/Microsoft.VCRedistVersion.default.txt")
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(_go_redist_arch x64)
    else()
        set(_go_redist_arch x86)
    endif()

    set(_go_msvcp "")
    if(EXISTS "${_go_redist_version_file}")
        file(STRINGS "${_go_redist_version_file}" _go_redist_dir_version LIMIT_COUNT 1)
        string(STRIP "${_go_redist_dir_version}" _go_redist_dir_version)
        file(GLOB _go_msvcp "${_go_vc_dir}/Redist/MSVC/${_go_redist_dir_version}/${_go_redist_arch}/Microsoft.VC*.CRT/msvcp140.dll")
    endif()

    set(_go_msvcp_version "")
    if(_go_msvcp)
        list(GET _go_msvcp 0 _go_msvcp)
        execute_process(
            COMMAND powershell -NoProfile -NonInteractive -Command "(Get-Item -LiteralPath '${_go_msvcp}').VersionInfo.FileVersion"
            OUTPUT_VARIABLE _go_msvcp_version
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET)
    endif()

    if(_go_msvcp_version MATCHES "^([0-9]+)\\.([0-9]+)\\.([0-9]+)")
        message(STATUS "VC++ runtime check requires ${CMAKE_MATCH_1}.${CMAKE_MATCH_2}.${CMAKE_MATCH_3} or newer")
        set_source_files_properties(VCRuntimeCheck.cpp PROPERTIES COMPILE_DEFINITIONS
            "GO_VCREDIST_MAJOR=${CMAKE_MATCH_1};GO_VCREDIST_MINOR=${CMAKE_MATCH_2};GO_VCREDIST_BUILD=${CMAKE_MATCH_3}")
    else()
        message(WARNING "Could not read the toolset's msvcp140.dll version, the VC++ runtime check falls back to the compiler version")
    endif()

    # New STL features live in satellite DLLs older redistributables lack, and a missing import stops the
    # loader before any game code runs. Delay-load them so VCRuntimeCheck.cpp can detect and install instead.
    # LNK4199 is the harmless "no imports found" warning for satellites the exe doesn't use (yet).
    target_link_options(z_generals PRIVATE
        "/DELAYLOAD:msvcp140_atomic_wait.dll"
        "/DELAYLOAD:msvcp140_1.dll"
        "/DELAYLOAD:msvcp140_2.dll"
        "/DELAYLOAD:msvcp140_codecvt_ids.dll"
        "/IGNORE:4199"
    )
    target_link_libraries(z_generals PRIVATE delayimp)
endif()

if(WIN32)
	add_custom_command(TARGET z_generals POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
			"${CMAKE_CURRENT_SOURCE_DIR}/../GameEngine/Source/GameNetwork/GeneralsOnline/Vendor/DiscordRPC/discord-rpc.dll"
			"$<TARGET_FILE_DIR:z_generals>/discord-rpc.dll"
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
			"${CMAKE_CURRENT_SOURCE_DIR}/../GameEngine/Source/GameNetwork/GeneralsOnline/Vendor/DiscordRPC/LICENSE.txt"
			"$<TARGET_FILE_DIR:z_generals>/discord-rpc-LICENSE.txt"
	)

	if(EXISTS "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/tools/sentry-native/crashpad_handler.exe")
		# sentry's crashpad backend is an out-of-process watcher: it needs its
		# own executable next to the game, at the default handler path sentry
		# looks for (see sentry_options_set_handler_path in sentry.h).
		add_custom_command(TARGET z_generals POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy_if_different
				"${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/tools/sentry-native/crashpad_handler.exe"
				"$<TARGET_FILE_DIR:z_generals>/crashpad_handler.exe"
		)
	endif()
endif()

# RC files optional for MinGW builds
# Icon: LoadIcon() handles missing resource gracefully (uses default system icon)
# Manifest: DPI awareness metadata (nice to have but not essential)
if(MSVC)
    # VS2005 and later adds default manifest, we need to turn it off to prevent conflict with custom manifest
    if(NOT IS_VS6_BUILD)
        target_link_options(z_generals PRIVATE
            "/MANIFEST:NO"
        )
    endif()

    target_sources(z_generals PRIVATE
        RTS.RC
    )
endif()

# Strip debug symbols to separate file for MinGW Release builds
# This creates generalszh${RTS_BUILD_OUTPUT_SUFFIX}.exe.debug (similar to MSVC .pdb files)
if(MINGW AND COMMAND add_debug_strip_target)
    add_debug_strip_target(z_generals)
endif()
