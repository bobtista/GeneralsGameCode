set(GAMEENGINEDEVICE_SRC
#    Include/MilesAudioDevice/MilesAudioManager.h
#    Include/VideoDevice/Bink/BinkVideoPlayer.h
#    Include/W3DDevice/Common/W3DConvert.h
#    Include/W3DDevice/Common/W3DFunctionLexicon.h
#    Include/W3DDevice/Common/W3DModuleFactory.h
#    Include/W3DDevice/Common/W3DRadar.h
#    Include/W3DDevice/Common/W3DThingFactory.h
#    Include/W3DDevice/GameClient/BaseHeightMap.h
#    Include/W3DDevice/GameClient/camerashakesystem.h
#    Include/W3DDevice/GameClient/FlatHeightMap.h
#    Include/W3DDevice/GameClient/HeightMap.h
#    Include/W3DDevice/GameClient/Module/W3DDebrisDraw.h
#    Include/W3DDevice/GameClient/Module/W3DDefaultDraw.h
#    Include/W3DDevice/GameClient/Module/W3DDependencyModelDraw.h
#    Include/W3DDevice/GameClient/Module/W3DLaserDraw.h
#    Include/W3DDevice/GameClient/Module/W3DModelDraw.h
#    Include/W3DDevice/GameClient/Module/W3DOverlordAircraftDraw.h
#    Include/W3DDevice/GameClient/Module/W3DOverlordTankDraw.h
#    Include/W3DDevice/GameClient/Module/W3DOverlordTruckDraw.h
#    Include/W3DDevice/GameClient/Module/W3DPoliceCarDraw.h
#    Include/W3DDevice/GameClient/Module/W3DProjectileStreamDraw.h
#    Include/W3DDevice/GameClient/Module/W3DPropDraw.h
#    Include/W3DDevice/GameClient/Module/W3DRopeDraw.h
#    Include/W3DDevice/GameClient/Module/W3DScienceModelDraw.h
#    Include/W3DDevice/GameClient/Module/W3DSupplyDraw.h
#    Include/W3DDevice/GameClient/Module/W3DTankDraw.h
#    Include/W3DDevice/GameClient/Module/W3DTankTruckDraw.h
#    Include/W3DDevice/GameClient/Module/W3DTracerDraw.h
#    Include/W3DDevice/GameClient/Module/W3DTreeDraw.h
#    Include/W3DDevice/GameClient/Module/W3DTruckDraw.h
#    Include/W3DDevice/GameClient/TerrainTex.h
#    Include/W3DDevice/GameClient/TileData.h
    Include/W3DDevice/GameClient/W3DAssetManager.h
    Include/W3DDevice/GameClient/W3DAssetManagerExposed.h
    Include/W3DDevice/GameClient/W3DBibBuffer.h
    Include/W3DDevice/GameClient/W3DBridgeBuffer.h
    Include/W3DDevice/GameClient/W3DBufferManager.h
    Include/W3DDevice/GameClient/W3DCustomScene.h
#    Include/W3DDevice/GameClient/W3DDebugDisplay.h
    Include/W3DDevice/GameClient/R3DDebugPanel.h
    Include/W3DDevice/GameClient/R3DDebugGrid.h
#    Include/W3DDevice/GameClient/W3DDebugIcons.h
#    Include/W3DDevice/GameClient/W3DDisplay.h
#    Include/W3DDevice/GameClient/W3DDisplayString.h
#    Include/W3DDevice/GameClient/W3DDisplayStringManager.h
    Include/W3DDevice/GameClient/W3DDynamicLight.h
    Include/W3DDevice/GameClient/W3DFileSystem.h
#    Include/W3DDevice/GameClient/W3DGadget.h
    Include/W3DDevice/GameClient/W3DGameClient.h
#    Include/W3DDevice/GameClient/W3DGameFont.h
#    Include/W3DDevice/GameClient/W3DGameWindow.h
#    Include/W3DDevice/GameClient/W3DGameWindowManager.h
#    Include/W3DDevice/GameClient/W3DGUICallbacks.h
    Include/W3DDevice/GameClient/W3DInGameUI.h
    Include/W3DDevice/GameClient/R3DLocalLights.h
    Include/W3DDevice/GameClient/W3DMirror.h
#    Include/W3DDevice/GameClient/W3DMouse.h
#    Include/W3DDevice/GameClient/W3DParticleSys.h
    Include/W3DDevice/GameClient/W3DPoly.h
    Include/W3DDevice/GameClient/W3DProjectedShadow.h
#    Include/W3DDevice/GameClient/W3DPropBuffer.h
    Include/W3DDevice/GameClient/W3DRoadBuffer.h
    Include/W3DDevice/GameClient/W3DScene.h
#    Include/W3DDevice/GameClient/W3DShaderManager.h
    Include/W3DDevice/GameClient/W3DShadow.h
    Include/W3DDevice/GameClient/W3DShroud.h
    Include/W3DDevice/GameClient/R3DSkyCube.h
    Include/W3DDevice/GameClient/R3DSsao.h
    Include/W3DDevice/GameClient/R3DTaa.h
    Include/W3DDevice/GameClient/R3DTreeFire.h
    # Ronin @feature 28/10/2026 DX9: the RmlUi layer (docs/UI_Work.md)
    Include/W3DDevice/GameClient/RmlUi/RmlUiFileInterface.h
    Include/W3DDevice/GameClient/RmlUi/RmlUiManager.h
    Include/W3DDevice/GameClient/RmlUi/RmlUiRenderInterface.h
    Include/W3DDevice/GameClient/RmlUi/RmlUiSystemInterface.h
    Include/W3DDevice/GameClient/RmlUi/RmlControlBarScreen.h
    Include/W3DDevice/GameClient/RmlUi/RmlUiElements.h
    Include/W3DDevice/GameClient/RmlUi/RmlUiText.h
    Include/W3DDevice/GameClient/RmlUi/RmlHudPanels.h
    # Ronin @feature 29/10/2026 DX9: the shell screens: main menu, game menu, Options
    Include/W3DDevice/GameClient/RmlUi/RmlScreen.h
    Include/W3DDevice/GameClient/RmlUi/RmlMainMenuScreen.h
    Include/W3DDevice/GameClient/RmlUi/RmlQuitMenuScreen.h
    Include/W3DDevice/GameClient/RmlUi/RmlOptionsScreen.h
    # Ronin @feature 30/10/2026 DX9: tintinhamans' single-player RmlUi screens and their logic (docs/UI_Work.md)
    Include/W3DDevice/GameClient/RmlUi/RmlChallengeMenuScreen.h
    Include/W3DDevice/GameClient/RmlUi/RmlCreditsScreen.h
    Include/W3DDevice/GameClient/RmlUi/RmlGrowOnlyList.h
    Include/W3DDevice/GameClient/RmlUi/RmlMapSearch.h
    Include/W3DDevice/GameClient/RmlUi/RmlPopupReplayScreen.h
    Include/W3DDevice/GameClient/RmlUi/RmlReplayMenuScreen.h
    Include/W3DDevice/GameClient/RmlUi/RmlSaveLoadScreen.h
    Include/W3DDevice/GameClient/RmlUi/RmlSkirmishMapSelectScreen.h
    Include/W3DDevice/GameClient/RmlUi/RmlSkirmishSetupScreen.h
    # Ronin @feature 31/10/2026 DX9: tintinhamans' RmlUi screens around a match and their logic (docs/UI_Work.md)
    Include/W3DDevice/GameClient/RmlUi/RmlChatInput.h
    Include/W3DDevice/GameClient/RmlUi/RmlDisconnectScreen.h
    Include/W3DDevice/GameClient/RmlUi/RmlDownloadScreen.h
    Include/W3DDevice/GameClient/RmlUi/RmlEndGameOverlayScreen.h
    Include/W3DDevice/GameClient/RmlUi/RmlInGamePopupScreen.h
    Include/W3DDevice/GameClient/RmlUi/RmlLoadScreen.h
    Include/W3DDevice/GameClient/RmlUi/RmlMessageBox.h
    Include/W3DDevice/GameClient/RmlUi/RmlScoreScreen.h
    # Ronin @feature 31/10/2026 DX9: tintinhamans' RmlUi LAN screens and their logic (docs/UI_Work.md)
    Include/W3DDevice/GameClient/RmlUi/RmlLanGameSetupScreen.h
    Include/W3DDevice/GameClient/RmlUi/RmlLanLobbyScreen.h
    Include/W3DDevice/GameClient/RmlUi/RmlLanMapSelectScreen.h
    Include/W3DDevice/GameClient/RmlUi/RmlNetworkDirectConnectScreen.h
#    Include/W3DDevice/GameClient/W3DSmudge.h
#    Include/W3DDevice/GameClient/W3DSnow.h
    Include/W3DDevice/GameClient/W3DStatusCircle.h
#    Include/W3DDevice/GameClient/W3DTerrainBackground.h
#    Include/W3DDevice/GameClient/W3DTerrainTracks.h
#    Include/W3DDevice/GameClient/W3DTerrainVisual.h
#    Include/W3DDevice/GameClient/W3DTreeBuffer.h
#    Include/W3DDevice/GameClient/W3DVideoBuffer.h
#    Include/W3DDevice/GameClient/W3DView.h
    Include/W3DDevice/GameClient/R3DShadowMap.h
    Include/W3DDevice/GameClient/W3DVolumetricShadow.h
#    Include/W3DDevice/GameClient/W3DWater.h
#    Include/W3DDevice/GameClient/W3DWaterTracks.h
    Include/W3DDevice/GameClient/W3DWaypointBuffer.h
    Include/W3DDevice/GameClient/W3DWebBrowser.h
#    Include/W3DDevice/GameClient/WorldHeightMap.h
    Include/W3DDevice/GameLogic/W3DGameLogic.h
    Include/W3DDevice/GameLogic/W3DGhostObject.h
    Include/W3DDevice/GameLogic/W3DTerrainLogic.h
#    Include/Win32Device/Common/Win32GameEngine.h
#    Include/Win32Device/Common/Win32LocalFile.h
#    Include/Win32Device/Common/Win32LocalFileSystem.h
#    Include/Win32Device/GameClient/Win32DIKeyboard.h
#    Include/Win32Device/GameClient/Win32DIMouse.h
#    Include/Win32Device/GameClient/Win32Mouse.h
#    Source/MilesAudioDevice/MilesAudioManager.cpp
#    Source/VideoDevice/Bink/BinkVideoPlayer.cpp
#    Source/W3DDevice/Common/System/W3DFunctionLexicon.cpp
#    Source/W3DDevice/Common/System/W3DRadar.cpp
#    Source/W3DDevice/Common/Thing/W3DModuleFactory.cpp
#    Source/W3DDevice/Common/Thing/W3DThingFactory.cpp
#    Source/W3DDevice/Common/W3DConvert.cpp
#    Source/W3DDevice/GameClient/BaseHeightMap.cpp
#    Source/W3DDevice/GameClient/camerashakesystem.cpp
#    Source/W3DDevice/GameClient/Drawable/Draw/W3DDebrisDraw.cpp
#    Source/W3DDevice/GameClient/Drawable/Draw/W3DDefaultDraw.cpp
#    Source/W3DDevice/GameClient/Drawable/Draw/W3DDependencyModelDraw.cpp
#    Source/W3DDevice/GameClient/Drawable/Draw/W3DLaserDraw.cpp
#    Source/W3DDevice/GameClient/Drawable/Draw/W3DModelDraw.cpp
#    Source/W3DDevice/GameClient/Drawable/Draw/W3DOverlordAircraftDraw.cpp
#    Source/W3DDevice/GameClient/Drawable/Draw/W3DOverlordTankDraw.cpp
#    Source/W3DDevice/GameClient/Drawable/Draw/W3DOverlordTruckDraw.cpp
#    Source/W3DDevice/GameClient/Drawable/Draw/W3DPoliceCarDraw.cpp
#    Source/W3DDevice/GameClient/Drawable/Draw/W3DProjectileStreamDraw.cpp
#    Source/W3DDevice/GameClient/Drawable/Draw/W3DPropDraw.cpp
#    Source/W3DDevice/GameClient/Drawable/Draw/W3DRopeDraw.cpp
#    Source/W3DDevice/GameClient/Drawable/Draw/W3DScienceModelDraw.cpp
#    Source/W3DDevice/GameClient/Drawable/Draw/W3DSupplyDraw.cpp
#    Source/W3DDevice/GameClient/Drawable/Draw/W3DTankDraw.cpp
#    Source/W3DDevice/GameClient/Drawable/Draw/W3DTankTruckDraw.cpp
#    Source/W3DDevice/GameClient/Drawable/Draw/W3DTracerDraw.cpp
#    Source/W3DDevice/GameClient/Drawable/Draw/W3DTreeDraw.cpp
#    Source/W3DDevice/GameClient/Drawable/Draw/W3DTruckDraw.cpp
#    Source/W3DDevice/GameClient/FlatHeightMap.cpp
#    Source/W3DDevice/GameClient/GUI/Gadget/W3DCheckBox.cpp
#    Source/W3DDevice/GameClient/GUI/Gadget/W3DComboBox.cpp
#    Source/W3DDevice/GameClient/GUI/Gadget/W3DHorizontalSlider.cpp
#    Source/W3DDevice/GameClient/GUI/Gadget/W3DListBox.cpp
#    Source/W3DDevice/GameClient/GUI/Gadget/W3DProgressBar.cpp
#    Source/W3DDevice/GameClient/GUI/Gadget/W3DPushButton.cpp
#    Source/W3DDevice/GameClient/GUI/Gadget/W3DRadioButton.cpp
#    Source/W3DDevice/GameClient/GUI/Gadget/W3DStaticText.cpp
#    Source/W3DDevice/GameClient/GUI/Gadget/W3DTabControl.cpp
#    Source/W3DDevice/GameClient/GUI/Gadget/W3DTextEntry.cpp
#    Source/W3DDevice/GameClient/GUI/Gadget/W3DVerticalSlider.cpp
#    Source/W3DDevice/GameClient/GUI/GUICallbacks/W3DControlBar.cpp
#    Source/W3DDevice/GameClient/GUI/GUICallbacks/W3DMainMenu.cpp
#    Source/W3DDevice/GameClient/GUI/GUICallbacks/W3DMOTD.cpp
#    Source/W3DDevice/GameClient/GUI/W3DGameFont.cpp
#    Source/W3DDevice/GameClient/GUI/W3DGameWindow.cpp
#    Source/W3DDevice/GameClient/GUI/W3DGameWindowManager.cpp
#    Source/W3DDevice/GameClient/HeightMap.cpp
    Source/W3DDevice/GameClient/Shadow/W3DBufferManager.cpp
    Source/W3DDevice/GameClient/Shadow/W3DProjectedShadow.cpp
    Source/W3DDevice/GameClient/Shadow/W3DShadow.cpp
    Source/W3DDevice/GameClient/Shadow/R3DShadowMap.cpp
    Source/W3DDevice/GameClient/Shadow/W3DVolumetricShadow.cpp
#    Source/W3DDevice/GameClient/TerrainTex.cpp
#    Source/W3DDevice/GameClient/TileData.cpp
    Source/W3DDevice/GameClient/W3DAssetManager.cpp
    Source/W3DDevice/GameClient/W3DAssetManagerExposed.cpp
    Source/W3DDevice/GameClient/W3DBibBuffer.cpp
    Source/W3DDevice/GameClient/W3DBridgeBuffer.cpp
#    Source/W3DDevice/GameClient/W3DDebugDisplay.cpp
    Source/W3DDevice/GameClient/R3DDebugGrid.cpp
    Source/W3DDevice/GameClient/R3DDebugPanel.cpp
#    Source/W3DDevice/GameClient/W3DDebugIcons.cpp
#    Source/W3DDevice/GameClient/W3DDisplay.cpp
#    Source/W3DDevice/GameClient/W3DDisplayString.cpp
#    Source/W3DDevice/GameClient/W3DDisplayStringManager.cpp
    Source/W3DDevice/GameClient/W3DDynamicLight.cpp
    Source/W3DDevice/GameClient/W3DFileSystem.cpp
    Source/W3DDevice/GameClient/W3DGameClient.cpp
    Source/W3DDevice/GameClient/W3DInGameUI.cpp
    Source/W3DDevice/GameClient/R3DLocalLights.cpp
#    Source/W3DDevice/GameClient/W3DMouse.cpp
#    Source/W3DDevice/GameClient/W3DParticleSys.cpp
    Source/W3DDevice/GameClient/W3DPoly.cpp
#    Source/W3DDevice/GameClient/W3DPropBuffer.cpp
    Source/W3DDevice/GameClient/W3DRoadBuffer.cpp
    Source/W3DDevice/GameClient/W3DScene.cpp
#    Source/W3DDevice/GameClient/W3DShaderManager.cpp
    Source/W3DDevice/GameClient/W3DShroud.cpp
    Source/W3DDevice/GameClient/R3DSkyCube.cpp
    Source/W3DDevice/GameClient/R3DSsao.cpp
    Source/W3DDevice/GameClient/R3DTaa.cpp
    Source/W3DDevice/GameClient/R3DTreeFire.cpp
    # Ronin @feature 28/10/2026 DX9: the RmlUi layer 
    Source/W3DDevice/GameClient/RmlUi/RmlUiFileInterface.cpp
    Source/W3DDevice/GameClient/RmlUi/RmlUiManager.cpp
    Source/W3DDevice/GameClient/RmlUi/RmlUiRenderInterface.cpp
    Source/W3DDevice/GameClient/RmlUi/RmlUiSystemInterface.cpp
    Source/W3DDevice/GameClient/RmlUi/RmlControlBarScreen.cpp
    Source/W3DDevice/GameClient/RmlUi/RmlUiElements.cpp
    Source/W3DDevice/GameClient/RmlUi/RmlHudPanels.cpp
    # Ronin @feature 29/10/2026 DX9: the shell screens: main menu, game menu, Options
    Source/W3DDevice/GameClient/RmlUi/RmlMainMenuScreen.cpp
    Source/W3DDevice/GameClient/RmlUi/RmlQuitMenuScreen.cpp
    Source/W3DDevice/GameClient/RmlUi/RmlOptionsScreen.cpp
    # Ronin @feature 30/10/2026 DX9: tintinhamans' single-player RmlUi screens and their logic (docs/UI_Work.md)
    Source/W3DDevice/GameClient/RmlUi/RmlChallengeMenuScreen.cpp
    Source/W3DDevice/GameClient/RmlUi/RmlCreditsScreen.cpp
    Source/W3DDevice/GameClient/RmlUi/RmlPopupReplayScreen.cpp
    Source/W3DDevice/GameClient/RmlUi/RmlReplayMenuScreen.cpp
    Source/W3DDevice/GameClient/RmlUi/RmlSaveLoadScreen.cpp
    Source/W3DDevice/GameClient/RmlUi/RmlSkirmishMapSelectScreen.cpp
    Source/W3DDevice/GameClient/RmlUi/RmlSkirmishSetupScreen.cpp
    # Ronin @feature 31/10/2026 DX9: tintinhamans' RmlUi screens around a match and their logic (docs/UI_Work.md)
    Source/W3DDevice/GameClient/RmlUi/RmlDisconnectScreen.cpp
    Source/W3DDevice/GameClient/RmlUi/RmlDownloadScreen.cpp
    Source/W3DDevice/GameClient/RmlUi/RmlEndGameOverlayScreen.cpp
    Source/W3DDevice/GameClient/RmlUi/RmlInGamePopupScreen.cpp
    Source/W3DDevice/GameClient/RmlUi/RmlLoadScreen.cpp
    Source/W3DDevice/GameClient/RmlUi/RmlMessageBox.cpp
    Source/W3DDevice/GameClient/RmlUi/RmlScoreScreen.cpp
    # Ronin @feature 31/10/2026 DX9: tintinhamans' RmlUi LAN screens and their logic (docs/UI_Work.md)
    Source/W3DDevice/GameClient/RmlUi/RmlLanGameSetupScreen.cpp
    Source/W3DDevice/GameClient/RmlUi/RmlLanLobbyScreen.cpp
    Source/W3DDevice/GameClient/RmlUi/RmlLanMapSelectScreen.cpp
    Source/W3DDevice/GameClient/RmlUi/RmlNetworkDirectConnectScreen.cpp
#    Source/W3DDevice/GameClient/W3DSmudge.cpp
#    Source/W3DDevice/GameClient/W3DSnow.cpp
    Source/W3DDevice/GameClient/W3DStatusCircle.cpp
#    Source/W3DDevice/GameClient/W3DTerrainBackground.cpp
#    Source/W3DDevice/GameClient/W3DTerrainTracks.cpp
#    Source/W3DDevice/GameClient/W3DTerrainVisual.cpp
#    Source/W3DDevice/GameClient/W3DTreeBuffer.cpp
#    Source/W3DDevice/GameClient/W3DVideoBuffer.cpp
#    Source/W3DDevice/GameClient/W3DView.cpp
    Source/W3DDevice/GameClient/W3dWaypointBuffer.cpp
    Source/W3DDevice/GameClient/W3DWebBrowser.cpp
#    Source/W3DDevice/GameClient/Water/W3DWater.cpp
#    Source/W3DDevice/GameClient/Water/W3DWaterTracks.cpp
#    Source/W3DDevice/GameClient/WorldHeightMap.cpp
    Source/W3DDevice/GameLogic/W3DGameLogic.cpp
    Source/W3DDevice/GameLogic/W3DGhostObject.cpp
    Source/W3DDevice/GameLogic/W3DTerrainLogic.cpp
#    Source/Win32Device/Common/Win32GameEngine.cpp
#    Source/Win32Device/Common/Win32LocalFile.cpp
#    Source/Win32Device/Common/Win32LocalFileSystem.cpp
#    Source/Win32Device/Common/Win32OSDisplay.cpp
#    Source/Win32Device/GameClient/Win32DIKeyboard.cpp
#    #Source/Win32Device/GameClient/Win32DIMouse.cpp
#    Source/Win32Device/GameClient/Win32Mouse.cpp
)

add_library(z_gameenginedevice STATIC)

target_sources(z_gameenginedevice PRIVATE ${GAMEENGINEDEVICE_SRC})

target_include_directories(z_gameenginedevice PUBLIC
    Include
)

target_precompile_headers(z_gameenginedevice PRIVATE
    [["Precompiled/BasePrecomp.h"]] # Must be first
    [["Common/STLTypedefs.h"]]
    [["Common/SubsystemInterface.h"]]
    [["WWLib/INI.h"]]
    [["WWLib/WWCommon.h"]]
    <windows.h>
)

target_link_libraries(z_gameenginedevice PRIVATE
    corei_gameenginedevice_private
    zi_always
    zi_main
    RmlUi::RmlUi    # Ronin @feature 28/10/2026 DX9: the RmlUi layer of the modern control bar
)

target_link_libraries(z_gameenginedevice PUBLIC
    corei_gameenginedevice_public
    z_gameengine
)
