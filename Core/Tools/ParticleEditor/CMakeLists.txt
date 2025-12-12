set(PARTICLEED_SRC
    "CButtonShowColor.cpp"
    "CButtonShowColor.h"
    "CColorAlphaDialog.cpp"
    "CColorAlphaDialog.h"
    "CSwitchesDialog.cpp"
    "CSwitchesDialog.h"
    "EmissionTypePanels.cpp"
    "EmissionTypePanels.h"
    "ISwapablePanel.h"
    "MoreParmsDialog.cpp"
    "MoreParmsDialog.h"
    "ParticleEditor.cpp"
    "ParticleEditor.h"
    "ParticleEditorDialog.cpp"
    "ParticleEditorDialog.h"
    "ParticleEditorExport.h"
    "ParticleTypePanels.cpp"
    "ParticleTypePanels.h"
    "Resource.h"
    "ShaderTypePanels.cpp"
    "ShaderTypePanels.h"
    "StdAfx.cpp"
    "StdAfx.h"
    "VelocityTypePanels.cpp"
    "VelocityTypePanels.h"
)

if(RTS_BUILD_GENERALS)
    set(PARTICLEEDITOR_TARGET g_particleeditor)
    set(PARTICLEEDITOR_LIBS
        gi_gameengine_include
        gi_always
        gi_libraries_source_wwvegas
    )
else()
    set(PARTICLEEDITOR_TARGET z_particleeditor)
    set(PARTICLEEDITOR_LIBS
        zi_gameengine_include
        zi_always
        zi_libraries_source_wwvegas
        core_debug
        core_profile
    )
endif()

add_library(${PARTICLEEDITOR_TARGET} SHARED)
set_target_properties(${PARTICLEEDITOR_TARGET} PROPERTIES OUTPUT_NAME particleeditor PREFIX "")

target_sources(${PARTICLEEDITOR_TARGET} PRIVATE ${PARTICLEED_SRC})

target_include_directories(${PARTICLEEDITOR_TARGET} PRIVATE
    include
    res
)

target_link_libraries(${PARTICLEEDITOR_TARGET} PRIVATE
    corei_libraries_source_wwvegas
    corei_libraries_source_wwvegas_wwlib
    d3d8lib
    imm32
    core_config
    stlport
    vfw32
    winmm
    ${PARTICLEEDITOR_LIBS}
)

if(WIN32 OR "${CMAKE_SYSTEM}" MATCHES "Windows")
    target_link_options(${PARTICLEEDITOR_TARGET} PRIVATE /NODEFAULTLIB:libci.lib /NODEFAULTLIB:libc.lib)
    target_compile_definitions(${PARTICLEEDITOR_TARGET} PRIVATE _AFXDLL)
    target_sources(${PARTICLEEDITOR_TARGET} PRIVATE ParticleEditor.rc)
    set_target_properties(${PARTICLEEDITOR_TARGET} PROPERTIES OUTPUT_NAME ParticleEditor)
else()
    set_target_properties(${PARTICLEEDITOR_TARGET} PROPERTIES OUTPUT_NAME particleeditor)
endif()

