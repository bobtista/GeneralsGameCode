add_executable(z_w3dview WIN32)

target_link_libraries(z_w3dview PRIVATE
    core_config
    core_utility
    core_wwstub # avoid linking GameEngine
    corei_w3dview # this interface gets the source files for the tool
    d3d8
    d3d8lib
    d3dx8
    imm32
    milesstub
    Version
    vfw32
    winmm
    z_wwaudio
    z_wwvegas
)

# Link Qt if available (required for Qt UI)
# TheSuperHackers @refactor bobtista 01/01/2025 MSVC 2022 compatibility: WWVegas uses ANSI strings
# Disable Unicode for z_w3dview to match WWVegas expectations (Qt can work with ANSI)
if(TARGET core_qt)
    target_link_libraries(z_w3dview PRIVATE core_qt)
    # Disable Unicode defines from Qt since WWVegas requires ANSI
    set_target_properties(z_w3dview PROPERTIES QT_NO_UNICODE_DEFINES TRUE)
    if(MSVC)
        target_compile_options(z_w3dview PRIVATE /UUNICODE /U_UNICODE)
    endif()
endif()

# Define HAVE_WWVEGAS so source files use real WWVegas headers instead of stubs
target_compile_definitions(z_w3dview PRIVATE
    HAVE_WWVEGAS=1
)

# TheSuperHackers @refactor bobtista 01/01/2025 MSVC 2022 compatibility workaround
# MSVC 2022 has false "duplicate definition" errors for inline functions with default arguments
# Use /permissive- to allow non-conforming code (wwstring.h was written for VC6)
if(MSVC AND NOT IS_VS6_BUILD)
    if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "19.00")
        # MSVC 2015+ - add /permissive- to work around wwstring.h compatibility issues
        target_compile_options(z_w3dview PRIVATE /permissive-)
    endif()
endif()

if(WIN32 OR "${CMAKE_SYSTEM}" MATCHES "Windows")
    target_compile_definitions(z_w3dview PRIVATE _AFXDLL)
    set_target_properties(z_w3dview PROPERTIES OUTPUT_NAME W3DViewZH)
    
    # TheSuperHackers @refactor bobtista 01/01/2025 Copy mss32.dll to output directory for runtime
    # mss32.dll is built by the milesstub target and needs to be available at runtime
    add_custom_command(TARGET z_w3dview POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:milesstub>"
            "$<TARGET_FILE_DIR:z_w3dview>/mss32.dll"
        COMMENT "Copying mss32.dll to output directory"
    )
    
    # TheSuperHackers @refactor bobtista 01/01/2025 Copy Qt platform plugins to output directory
    # Qt applications require platform plugins (qwindows.dll) in a 'platforms' subdirectory
    if(TARGET core_qt)
        # Determine Qt plugin path based on build configuration
        set(QT_PLUGIN_DIR "$<IF:$<CONFIG:Debug>,${CMAKE_BINARY_DIR}/vcpkg_installed/x86-windows/debug/Qt6/plugins/platforms,${CMAKE_BINARY_DIR}/vcpkg_installed/x86-windows/Qt6/plugins/platforms>")
        
        add_custom_command(TARGET z_w3dview POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:z_w3dview>/platforms"
            COMMENT "Creating platforms directory for Qt plugins"
        )
        
        # Copy the Windows platform plugin (required for Qt applications on Windows)
        add_custom_command(TARGET z_w3dview POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<IF:$<CONFIG:Debug>,${CMAKE_BINARY_DIR}/vcpkg_installed/x86-windows/debug/Qt6/plugins/platforms/qwindowsd.dll,${CMAKE_BINARY_DIR}/vcpkg_installed/x86-windows/Qt6/plugins/platforms/qwindows.dll>"
                "$<TARGET_FILE_DIR:z_w3dview>/platforms/$<IF:$<CONFIG:Debug>,qwindowsd.dll,qwindows.dll>"
            COMMENT "Copying Qt Windows platform plugin"
        )
    endif()
else()
    set_target_properties(z_w3dview PROPERTIES OUTPUT_NAME w3dviewzh)
endif()
