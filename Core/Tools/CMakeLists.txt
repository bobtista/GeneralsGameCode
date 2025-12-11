# Build tools

# Build useful tool binaries.
if(RTS_BUILD_CORE_TOOLS)
    add_subdirectory(DebugWindow)
    # W3DView is only built for Core if not building game-specific tools
    # (game-specific builds provide the interface library)
    if(NOT RTS_BUILD_GENERALS_TOOLS AND NOT RTS_BUILD_ZEROHOUR_TOOLS)
        add_subdirectory(W3DView)
    endif()
endif()

# Build less useful tool/test binaries.
if(RTS_BUILD_CORE_EXTRAS)
    add_subdirectory(assetcull)
    add_subdirectory(Babylon)
    add_subdirectory(buildVersionUpdate)
    add_subdirectory(Compress)
    add_subdirectory(CRCDiff)
    add_subdirectory(mangler)
    add_subdirectory(matchbot)
    add_subdirectory(textureCompress)
    add_subdirectory(timingTest)
    add_subdirectory(versionUpdate)
    add_subdirectory(wolSetup)
    add_subdirectory(WW3D)
endif()

# Add library interfaces here
# W3DView interface library is always needed for game-specific builds
if(RTS_BUILD_GENERALS_TOOLS OR RTS_BUILD_ZEROHOUR_TOOLS)
    add_subdirectory(ImagePacker)
    add_subdirectory(MapCacheBuilder)
    add_subdirectory(W3DView)  # Provides corei_w3dview interface library
endif()

# Add library interfaces here
if(RTS_BUILD_GENERALS_EXTRAS OR RTS_BUILD_ZEROHOUR_EXTRAS)
    add_subdirectory(Autorun)
    add_subdirectory(Launcher)
    add_subdirectory(PATCHGET)
endif()
