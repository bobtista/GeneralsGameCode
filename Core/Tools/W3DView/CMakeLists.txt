# TheSuperHackers @refactor bobtista 01/01/2025 Migrated from MFC to Qt with VC6 fallback
# Check if this is a VC6 build (VC6 doesn't support Qt, needs MFC)
if(DEFINED MSVC_VERSION AND MSVC_VERSION LESS 1300)
    set(IS_VS6_BUILD TRUE)
else()
    set(IS_VS6_BUILD FALSE)
endif()

# Qt files - only for non-VC6 builds (VC6 doesn't support Qt)
set(W3DVIEW_QT_SRC
    "GameEngineStubs.h"
    "MainFrm_Qt.cpp"
    "MainFrm_Qt.h"
    "W3DView_Qt.cpp"
    "W3DView_Qt.h"
    "DataTreeView_Qt.cpp"
    "DataTreeView_Qt.h"
    "GraphicView_Qt.cpp"
    "GraphicView_Qt.h"
    "ResolutionDialog_Qt.cpp"
    "ResolutionDialog_Qt.h"
    "DeviceSelectionDialog_Qt.cpp"
    "DeviceSelectionDialog_Qt.h"
    "ScaleDialog_Qt.cpp"
    "ScaleDialog_Qt.h"
    "GammaDialog_Qt.cpp"
    "GammaDialog_Qt.h"
    "SaveSettingsDialog_Qt.cpp"
    "SaveSettingsDialog_Qt.h"
    "CameraDistanceDialog_Qt.cpp"
    "CameraDistanceDialog_Qt.h"
    "AmbientLightDialog_Qt.cpp"
    "AmbientLightDialog_Qt.h"
    "BackgroundColorDialog_Qt.cpp"
    "BackgroundColorDialog_Qt.h"
    "BackgroundBMPDialog_Qt.cpp"
    "BackgroundBMPDialog_Qt.h"
    "BackgroundObjectDialog_Qt.cpp"
    "BackgroundObjectDialog_Qt.h"
    "CameraSettingsDialog_Qt.cpp"
    "CameraSettingsDialog_Qt.h"
    "TexturePathDialog_Qt.cpp"
    "TexturePathDialog_Qt.h"
    "AssetPropertySheet_Qt.cpp"
    "AssetPropertySheet_Qt.h"
    "EmitterPropertySheet_Qt.cpp"
    "EmitterPropertySheet_Qt.h"
    "RingPropertySheet_Qt.cpp"
    "RingPropertySheet_Qt.h"
    "SpherePropertySheet_Qt.cpp"
    "SpherePropertySheet_Qt.h"
    "AdvancedAnimSheet_Qt.cpp"
    "AdvancedAnimSheet_Qt.h"
    "AggregateNameDialog_Qt.cpp"
    "AggregateNameDialog_Qt.h"
    "AddToLineupDialog_Qt.cpp"
    "AddToLineupDialog_Qt.h"
    "AnimationSpeed_Qt.cpp"
    "AnimationSpeed_Qt.h"
    "PlaySoundDialog_Qt.cpp"
    "PlaySoundDialog_Qt.h"
    "AnimatedSoundOptionsDialog_Qt.cpp"
    "AnimatedSoundOptionsDialog_Qt.h"
    "ParticleSizeDialog_Qt.cpp"
    "ParticleSizeDialog_Qt.h"
    "ParticleFrameKeyDialog_Qt.cpp"
    "ParticleFrameKeyDialog_Qt.h"
    "ParticleRotationKeyDialog_Qt.cpp"
    "ParticleRotationKeyDialog_Qt.h"
    "ParticleBlurTimeKeyDialog_Qt.cpp"
    "ParticleBlurTimeKeyDialog_Qt.h"
    "OpacitySettingsDialog_Qt.cpp"
    "OpacitySettingsDialog_Qt.h"
    "OpacityVectorDialog_Qt.cpp"
    "OpacityVectorDialog_Qt.h"
    "VolumeRandomDialog_Qt.cpp"
    "VolumeRandomDialog_Qt.h"
    "SoundEditDialog_Qt.cpp"
    "SoundEditDialog_Qt.h"
    "EditLODDialog_Qt.cpp"
    "EditLODDialog_Qt.h"
    "SceneLightDialog_Qt.cpp"
    "SceneLightDialog_Qt.h"
    "BoneMgrDialog_Qt.cpp"
    "BoneMgrDialog_Qt.h"
    "ColorSelectionDialog_Qt.cpp"
    "ColorSelectionDialog_Qt.h"
    "ColorPickerDialogClass_Qt.cpp"
    "ColorPickerDialogClass_Qt.h"
    "ColorPicker_Qt.cpp"
    "ColorPicker_Qt.h"
    "ColorBar_Qt.cpp"
    "ColorBar_Qt.h"
    "Vector3RndCombo_Qt.cpp"
    "Vector3RndCombo_Qt.h"
    "DialogToolbar_Qt.cpp"
    "DialogToolbar_Qt.h"
    "MeshPropPage_Qt.cpp"
    "MeshPropPage_Qt.h"
    "HierarchyPropPage_Qt.cpp"
    "HierarchyPropPage_Qt.h"
    "AnimationPropPage_Qt.cpp"
    "AnimationPropPage_Qt.h"
    "W3DViewDoc_Qt.cpp"
    "W3DViewDoc_Qt.h"
)

# MFC files - only for VC6 builds or when Qt is not available
set(W3DVIEW_MFC_SRC
    "W3DView.cpp"
    "W3DView.h"
    "MainFrm.cpp"
    "MainFrm.h"
    "DataTreeView.cpp"
    "DataTreeView.h"
    "GraphicView.cpp"
    "GraphicView.h"
    "W3DViewDoc.cpp"
    "W3DViewDoc.h"
    "W3DViewView.cpp"
    "W3DViewView.h"
    "ResolutionDialog.cpp"
    "ResolutionDialog.h"
    "DeviceSelectionDialog.cpp"
    "DeviceSelectionDialog.h"
    "ScaleDialog.cpp"
    "ScaleDialog.h"
    "GammaDialog.cpp"
    "GammaDialog.h"
    "SaveSettingsDialog.cpp"
    "SaveSettingsDialog.h"
    "CameraDistanceDialog.cpp"
    "CameraDistanceDialog.h"
    "AmbientLightDialog.cpp"
    "AmbientLightDialog.h"
    "BackgroundColorDialog.cpp"
    "BackgroundColorDialog.h"
    "BackgroundBMPDialog.cpp"
    "BackgroundBMPDialog.h"
    "BackgroundObjectDialog.cpp"
    "BackgroundObjectDialog.h"
    "CameraSettingsDialog.cpp"
    "CameraSettingsDialog.h"
    "TexturePathDialog.cpp"
    "TexturePathDialog.h"
    "TextureMgrDialog.cpp"
    "TextureMgrDialog.h"
    "TextureSettingsDialog.cpp"
    "TextureSettingsDialog.h"
    "AssetPropertySheet.cpp"
    "AssetPropertySheet.h"
    "EmitterPropertySheet.cpp"
    "EmitterPropertySheet.h"
    "RingPropertySheet.cpp"
    "RingPropertySheet.h"
    "SpherePropertySheet.cpp"
    "SpherePropertySheet.h"
    "AdvancedAnimSheet.cpp"
    "AdvancedAnimSheet.h"
    "AggregateNameDialog.cpp"
    "AggregateNameDialog.h"
    "AddToLineupDialog.cpp"
    "AddToLineupDialog.h"
    "AnimationSpeed.cpp"
    "AnimationSpeed.h"
    "PlaySoundDialog.cpp"
    "PlaySoundDialog.h"
    "AnimatedSoundOptionsDialog.cpp"
    "AnimatedSoundOptionsDialog.h"
    "ParticleSizeDialog.cpp"
    "ParticleSizeDialog.h"
    "ParticleFrameKeyDialog.cpp"
    "ParticleFrameKeyDialog.h"
    "ParticleRotationKeyDialog.cpp"
    "ParticleRotationKeyDialog.h"
    "ParticleBlurTimeKeyDialog.cpp"
    "ParticleBlurTimeKeyDialog.h"
    "OpacitySettingsDialog.cpp"
    "OpacitySettingsDialog.h"
    "OpacityVectorDialog.cpp"
    "OpacityVectorDialog.h"
    "VolumeRandomDialog.cpp"
    "VolumeRandomDialog.h"
    "SoundEditDialog.cpp"
    "SoundEditDialog.h"
    "EditLODDialog.cpp"
    "EditLODDialog.h"
    "SceneLightDialog.cpp"
    "SceneLightDialog.h"
    "BoneMgrDialog.cpp"
    "BoneMgrDialog.h"
    "ColorSelectionDialog.cpp"
    "ColorSelectionDialog.h"
    "ColorPickerDialogClass.cpp"
    "ColorPickerDialogClass.h"
    "ColorPicker.cpp"
    "ColorPicker.h"
    "ColorBar.cpp"
    "ColorBar.h"
    "Vector3RndCombo.cpp"
    "Vector3RndCombo.h"
    "DialogToolbar.cpp"
    "DialogToolbar.h"
    "Toolbar.cpp"
    "Toolbar.h"
    "MeshPropPage.cpp"
    "MeshPropPage.h"
    "HierarchyPropPage.cpp"
    "HierarchyPropPage.h"
    "AnimationPropPage.cpp"
    "AnimationPropPage.h"
    "EmitterColorPropPage.cpp"
    "EmitterColorPropPage.h"
    "EmitterFramePropPage.cpp"
    "EmitterFramePropPage.h"
    "EmitterGeneralPropPage.cpp"
    "EmitterGeneralPropPage.h"
    "EmitterLineGroupPropPage.cpp"
    "EmitterLineGroupPropPage.h"
    "EmitterLinePropPage.cpp"
    "EmitterLinePropPage.h"
    "EmitterParticlePropPage.cpp"
    "EmitterParticlePropPage.h"
    "EmitterPhysicsPropPage.cpp"
    "EmitterPhysicsPropPage.h"
    "EmitterRotationPropPage.cpp"
    "EmitterRotationPropPage.h"
    "EmitterSizePropPage.cpp"
    "EmitterSizePropPage.h"
    "EmitterUserPropPage.cpp"
    "EmitterUserPropPage.h"
    "RingColorPropPage.cpp"
    "RingColorPropPage.h"
    "RingGeneralPropPage.cpp"
    "RingGeneralPropPage.h"
    "RingSizePropPage.cpp"
    "RingSizePropPage.h"
    "SphereColorPropPage.cpp"
    "SphereColorPropPage.h"
    "SphereGeneralPropPage.cpp"
    "SphereGeneralPropPage.h"
    "SphereSizePropPage.cpp"
    "SphereSizePropPage.h"
    "AnimMixingPage.cpp"
    "AnimMixingPage.h"
    "AnimReportPage.cpp"
    "AnimReportPage.h"
    "DirectoryDialog.cpp"
    "DirectoryDialog.h"
    "RestrictedFileDialog.cpp"
    "RestrictedFileDialog.h"
    "StdAfx.cpp"
    "StdAfx.h"
)

# Non-MFC files (always included - shared code, utilities, etc.)
set(W3DVIEW_COMMON_SRC
    "AssetInfo.cpp"
    "AssetInfo.h"
    "AssetTypes.h"
    "ColorUtils.cpp"
    "ColorUtils.h"
    "Globals.cpp"
    "Globals.h"
    "LODDefs.h"
    "Utils.cpp"
    "Utils.h"
    "resource.h"
)

# Game engine dependent files (Windows-only)
if(WIN32 OR "${CMAKE_SYSTEM}" MATCHES "Windows")
    list(APPEND W3DVIEW_COMMON_SRC
        "EmitterInstanceList.cpp"
        "EmitterInstanceList.h"
        "ScreenCursor.cpp"
        "ScreenCursor.h"
        "ViewerAssetMgr.cpp"
        "ViewerAssetMgr.h"
        "ViewerScene.cpp"
        "ViewerScene.h"
    )
else()
    # Stub implementations for non-Windows
    list(APPEND W3DVIEW_COMMON_SRC
        "ViewerScene.h"
        "ViewerScene_Stubs.cpp"
    )
endif()

# Combine files based on build type
if(NOT IS_VS6_BUILD AND TARGET core_qt)
    # Qt build - use Qt files
    set(W3DVIEW_SRC ${W3DVIEW_QT_SRC} ${W3DVIEW_COMMON_SRC})
else()
    # MFC build (VC6 or no Qt) - use MFC files
    set(W3DVIEW_SRC ${W3DVIEW_MFC_SRC} ${W3DVIEW_COMMON_SRC})
endif()

# Interface library to provide source files to Generals/GeneralsMD builds
add_library(corei_w3dview INTERFACE)

target_sources(corei_w3dview INTERFACE ${W3DVIEW_SRC})

target_link_libraries(corei_w3dview INTERFACE
    core_config
    core_utility
)

if(NOT IS_VS6_BUILD AND TARGET core_qt)
    target_link_libraries(corei_w3dview INTERFACE core_qt)
endif()

# Add include directories so source files can find headers
target_include_directories(corei_w3dview INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

if(WIN32 OR "${CMAKE_SYSTEM}" MATCHES "Windows")
    target_sources(corei_w3dview INTERFACE W3DView.rc)
endif()

# Core-only executable (stubbed, no WWVegas)
if(RTS_BUILD_CORE_TOOLS)
    add_executable(core_w3dview)

    target_sources(core_w3dview PRIVATE ${W3DVIEW_SRC})

    if(NOT IS_VS6_BUILD AND TARGET core_qt)
        target_link_libraries(core_w3dview PRIVATE core_qt)
    endif()

    set_target_properties(core_w3dview PROPERTIES OUTPUT_NAME w3dview)

    if(WIN32 OR "${CMAKE_SYSTEM}" MATCHES "Windows")
        target_sources(core_w3dview PRIVATE W3DView.rc)
    endif()
endif()
