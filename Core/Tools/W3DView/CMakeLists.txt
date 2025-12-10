# Qt files (always included)
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
    "Toolbar.cpp"
    "Toolbar.h"
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

# Combine Qt and common files
set(W3DVIEW_SRC ${W3DVIEW_QT_SRC} ${W3DVIEW_COMMON_SRC})

add_executable(core_w3dview)

target_sources(core_w3dview PRIVATE ${W3DVIEW_SRC})

if(TARGET core_qt)
    target_link_libraries(core_w3dview PRIVATE core_qt)
endif()

# Precompiled headers removed - Qt-based code doesn't need MFC precompiled headers

set_target_properties(core_w3dview PROPERTIES OUTPUT_NAME w3dview)

if(WIN32 OR "${CMAKE_SYSTEM}" MATCHES "Windows")
    target_sources(core_w3dview PRIVATE W3DView.rc)
endif()
