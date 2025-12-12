# Created with python 3.11.4

# This script helps with moving cpp files from Generals or GeneralsMD to Core

import os
import shutil
from enum import Enum


class Game(Enum):
    GENERALS = 0
    ZEROHOUR = 1
    CORE = 2


class CmakeModifyType(Enum):
    ADD_COMMENT = 0
    REMOVE_COMMENT = 1


current_dir = os.path.dirname(os.path.abspath(__file__))
root_dir = os.path.join(current_dir, "..", "..")
root_dir = os.path.normpath(root_dir)
core_dir = os.path.join(root_dir, "Core")
generals_dir = os.path.join(root_dir, "Generals", "Code")
generalsmd_dir = os.path.join(root_dir, "GeneralsMD", "Code")


def get_game_path(game: Game):
    if game == Game.GENERALS:
        return generals_dir
    elif game == Game.ZEROHOUR:
        return generalsmd_dir
    elif game == Game.CORE:
        return core_dir
    assert(0)


def get_opposite_game(game: Game):
    if game == Game.GENERALS:
        return Game.ZEROHOUR
    elif game == Game.ZEROHOUR:
        return Game.GENERALS
    assert(0)


def move_file(fromGame: Game, fromFile: str, toGame: Game, toFile: str):
    fromPath = os.path.join(get_game_path(fromGame), os.path.normpath(fromFile))
    toPath = os.path.join(get_game_path(toGame), os.path.normpath(toFile))
    os.makedirs(os.path.dirname(toPath), exist_ok=True)
    shutil.move(fromPath, toPath)


def delete_file(game: Game, path: str):
    os.remove(os.path.join(get_game_path(game), os.path.normpath(path)))


def modify_cmakelists(cmakeFile: str, searchString: str, type: CmakeModifyType):
    lines: list[str]
    with open(cmakeFile, 'r', encoding="ascii") as file:
        lines = file.readlines()

    with open(cmakeFile, 'w', encoding="ascii") as file:
        for index, line  in enumerate(lines):
            if searchString in line:
                if type == CmakeModifyType.ADD_COMMENT:
                    lines[index] = "#" + line
                else:
                    lines[index] = line.replace("#", "", 1)

        file.writelines(lines)


def unify_file(fromGame: Game, fromFile: str, toGame: Game, toFile: str):
    assert(toGame == Game.CORE)

    fromOppositeGame = get_opposite_game(fromGame)
    fromOppositeGamePath = get_game_path(fromOppositeGame)
    fromGamePath = get_game_path(fromGame)
    toGamePath = get_game_path(toGame)

    fromFirstFolderIndex = fromFile.find("/")
    toFirstFolderIndex = toFile.find("/")
    assert(fromFirstFolderIndex > 0)
    assert(toFirstFolderIndex > 0)

    fromFirstFolderName = fromFile[:fromFirstFolderIndex]
    toFirstFolderName = toFile[:toFirstFolderIndex]
    fromFileInCmake = fromFile[fromFirstFolderIndex+1:]
    toFileInCmake = toFile[toFirstFolderIndex+1:]

    fromOppositeCmakeFile = os.path.join(fromOppositeGamePath, fromFirstFolderName, "CMakeLists.txt")
    fromCmakeFile = os.path.join(fromGamePath, fromFirstFolderName, "CMakeLists.txt")
    toCmakeFile = os.path.join(toGamePath, toFirstFolderName, "CMakeLists.txt")

    modify_cmakelists(fromOppositeCmakeFile, fromFileInCmake, CmakeModifyType.ADD_COMMENT)
    modify_cmakelists(fromCmakeFile, fromFileInCmake, CmakeModifyType.ADD_COMMENT)
    modify_cmakelists(toCmakeFile, toFileInCmake, CmakeModifyType.REMOVE_COMMENT)

    delete_file(fromOppositeGame, fromFile)
    move_file(fromGame, fromFile, toGame, toFile)


def unify_move_file(fromGame: Game, fromFile: str, toGame: Game, toFile: str):
    assert(toGame == Game.CORE)

    fromGamePath = get_game_path(fromGame)
    toGamePath = get_game_path(toGame)

    fromFirstFolderIndex = fromFile.find("/")
    toFirstFolderIndex = toFile.find("/")
    assert(fromFirstFolderIndex > 0)
    assert(toFirstFolderIndex > 0)

    fromFirstFolderName = fromFile[:fromFirstFolderIndex]
    toFirstFolderName = toFile[:toFirstFolderIndex]
    fromFileInCmake = fromFile[fromFirstFolderIndex+1:]
    toFileInCmake = toFile[toFirstFolderIndex+1:]

    fromCmakeFile = os.path.join(fromGamePath, fromFirstFolderName, "CMakeLists.txt")
    toCmakeFile = os.path.join(toGamePath, toFirstFolderName, "CMakeLists.txt")

    modify_cmakelists(fromCmakeFile, fromFileInCmake, CmakeModifyType.ADD_COMMENT)
    modify_cmakelists(toCmakeFile, toFileInCmake, CmakeModifyType.REMOVE_COMMENT)

    move_file(fromGame, fromFile, toGame, toFile)


def main():

    #unify_file(Game.ZEROHOUR, "GameEngine/Include/Common/crc.h", Game.CORE, "GameEngine/Include/Common/crc.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Include/Common/CRCDebug.h", Game.CORE, "GameEngine/Include/Common/CRCDebug.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/Common/crc.cpp", Game.CORE, "GameEngine/Source/Common/crc.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/Common/CRCDebug.cpp", Game.CORE, "GameEngine/Source/Common/CRCDebug.cpp")

    #unify_file(Game.ZEROHOUR, "GameEngine/Include/Common/RandomValue.h", Game.CORE, "GameEngine/Include/Common/RandomValue.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Include/GameClient/ClientRandomValue.h", Game.CORE, "GameEngine/Include/GameClient/ClientRandomValue.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Include/GameLogic/LogicRandomValue.h", Game.CORE, "GameEngine/Include/GameLogic/LogicRandomValue.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/Common/RandomValue.cpp", Game.CORE, "GameEngine/Source/Common/RandomValue.cpp")

    #unify_file(Game.ZEROHOUR, "GameEngine/Include/Common/Debug.h", Game.CORE, "GameEngine/Include/Common/Debug.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/Common/System/Debug.cpp", Game.CORE, "GameEngine/Source/Common/System/Debug.cpp")

    #unify_file(Game.ZEROHOUR, "GameEngine/Include/GameClient/VideoPlayer.h", Game.CORE, "GameEngine/Include/GameClient/VideoPlayer.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/GameClient/VideoPlayer.cpp", Game.CORE, "GameEngine/Source/GameClient/VideoPlayer.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/GameClient/VideoStream.cpp", Game.CORE, "GameEngine/Source/GameClient/VideoStream.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngine/Include/GameClient/WindowVideoManager.h", Game.CORE, "GameEngine/Include/GameClient/WindowVideoManager.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/GameClient/GUI/WindowVideoManager.cpp", Game.CORE, "GameEngine/Source/GameClient/GUI/WindowVideoManager.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/Common/INI/INIVideo.cpp", Game.CORE, "GameEngine/Source/Common/INI/INIVideo.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngineDevice/Include/VideoDevice/Bink/BinkVideoPlayer.h", Game.CORE, "GameEngineDevice/Include/VideoDevice/Bink/BinkVideoPlayer.h")
    #unify_file(Game.ZEROHOUR, "GameEngineDevice/Source/VideoDevice/Bink/BinkVideoPlayer.cpp", Game.CORE, "GameEngineDevice/Source/VideoDevice/Bink/BinkVideoPlayer.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngineDevice/Include/W3DDevice/GameClient/W3DVideoBuffer.h", Game.CORE, "GameEngineDevice/Include/W3DDevice/GameClient/W3DVideoBuffer.h")
    #unify_file(Game.ZEROHOUR, "GameEngineDevice/Source/W3DDevice/GameClient/W3DVideoBuffer.cpp", Game.CORE, "GameEngineDevice/Source/W3DDevice/GameClient/W3DVideoBuffer.cpp")
    #unify_move_file(Game.ZEROHOUR, "GameEngineDevice/Include/VideoDevice/FFmpeg/FFmpegFile.h", Game.CORE, "GameEngineDevice/Include/VideoDevice/FFmpeg/FFmpegFile.h")
    #unify_move_file(Game.ZEROHOUR, "GameEngineDevice/Include/VideoDevice/FFmpeg/FFmpegVideoPlayer.h", Game.CORE, "GameEngineDevice/Include/VideoDevice/FFmpeg/FFmpegVideoPlayer.h")
    #unify_move_file(Game.ZEROHOUR, "GameEngineDevice/Source/VideoDevice/FFmpeg/FFmpegFile.cpp", Game.CORE, "GameEngineDevice/Source/VideoDevice/FFmpeg/FFmpegFile.cpp")
    #unify_move_file(Game.ZEROHOUR, "GameEngineDevice/Source/VideoDevice/FFmpeg/FFmpegVideoPlayer.cpp", Game.CORE, "GameEngineDevice/Source/VideoDevice/FFmpeg/FFmpegVideoPlayer.cpp")

    #unify_file(Game.ZEROHOUR, "GameEngine/Include/Common/GameMemory.h", Game.CORE, "GameEngine/Include/Common/GameMemory.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Include/Common/GameMemoryNull.h", Game.CORE, "GameEngine/Include/Common/GameMemoryNull.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/Common/System/GameMemory.cpp", Game.CORE, "GameEngine/Source/Common/System/GameMemory.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/Common/System/GameMemoryNull.cpp", Game.CORE, "GameEngine/Source/Common/System/GameMemoryNull.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/Common/System/MemoryInit.cpp", Game.CORE, "GameEngine/Source/Common/System/GameMemoryInit.cpp")
    #unify_move_file(Game.GENERALS, "GameEngine/Source/Common/System/GameMemoryInitDMA_Generals.inl", Game.CORE, "GameEngine/Source/Common/System/GameMemoryInitDMA_Generals.inl")
    #unify_move_file(Game.ZEROHOUR, "GameEngine/Source/Common/System/GameMemoryInitDMA_GeneralsMD.inl", Game.CORE, "GameEngine/Source/Common/System/GameMemoryInitDMA_GeneralsMD.inl")
    #unify_move_file(Game.GENERALS, "GameEngine/Source/Common/System/GameMemoryInitPools_Generals.inl", Game.CORE, "GameEngine/Source/Common/System/GameMemoryInitPools_Generals.inl")
    #unify_move_file(Game.ZEROHOUR, "GameEngine/Source/Common/System/GameMemoryInitPools_GeneralsMD.inl", Game.CORE, "GameEngine/Source/Common/System/GameMemoryInitPools_GeneralsMD.inl")

    #unify_file(Game.ZEROHOUR, "GameEngine/Include/Common/ObjectStatusTypes.h", Game.CORE, "GameEngine/Include/Common/ObjectStatusTypes.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Include/Common/Radar.h", Game.CORE, "GameEngine/Include/Common/Radar.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/Common/System/ObjectStatusTypes.cpp", Game.CORE, "GameEngine/Source/Common/System/ObjectStatusTypes.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/Common/System/Radar.cpp", Game.CORE, "GameEngine/Source/Common/System/Radar.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngineDevice/Include/W3DDevice/Common/W3DRadar.h", Game.CORE, "GameEngineDevice/Include/W3DDevice/Common/W3DRadar.h")
    #unify_file(Game.ZEROHOUR, "GameEngineDevice/Source/W3DDevice/Common/System/W3DRadar.cpp", Game.CORE, "GameEngineDevice/Source/W3DDevice/Common/System/W3DRadar.cpp")

    #unify_move_file(Game.ZEROHOUR, "GameEngine/Include/GameClient/Smudge.h", Game.CORE, "GameEngine/Include/GameClient/Smudge.h")
    #unify_move_file(Game.ZEROHOUR, "GameEngine/Source/GameClient/System/Smudge.cpp", Game.CORE, "GameEngine/Source/GameClient/System/Smudge.cpp")
    #unify_move_file(Game.ZEROHOUR, "GameEngineDevice/Include/W3DDevice/GameClient/W3DSmudge.h", Game.CORE, "GameEngineDevice/Include/W3DDevice/GameClient/W3DSmudge.h")
    #unify_move_file(Game.ZEROHOUR, "GameEngineDevice/Source/W3DDevice/GameClient/W3DSmudge.cpp", Game.CORE, "GameEngineDevice/Source/W3DDevice/GameClient/W3DSmudge.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngineDevice/Include/W3DDevice/GameClient/W3DShaderManager.h", Game.CORE, "GameEngineDevice/Include/W3DDevice/GameClient/W3DShaderManager.h")
    #unify_file(Game.ZEROHOUR, "GameEngineDevice/Source/W3DDevice/GameClient/W3DShaderManager.cpp", Game.CORE, "GameEngineDevice/Source/W3DDevice/GameClient/W3DShaderManager.cpp")

    #unify_move_file(Game.ZEROHOUR, "GameEngine/Include/GameClient/ParabolicEase.h", Game.CORE, "GameEngine/Include/GameClient/ParabolicEase.h")
    #unify_move_file(Game.ZEROHOUR, "GameEngine/Source/GameClient/ParabolicEase.cpp", Game.CORE, "GameEngine/Source/GameClient/ParabolicEase.cpp")
    #unify_move_file(Game.ZEROHOUR, "GameEngineDevice/Include/W3DDevice/GameClient/camerashakesystem.h", Game.CORE, "GameEngineDevice/Include/W3DDevice/GameClient/CameraShakeSystem.h")
    #unify_move_file(Game.ZEROHOUR, "GameEngineDevice/Source/W3DDevice/GameClient/camerashakesystem.cpp", Game.CORE, "GameEngineDevice/Source/W3DDevice/GameClient/CameraShakeSystem.cpp")

    #unify_file(Game.ZEROHOUR, "GameEngine/Include/GameClient/View.h", Game.CORE, "GameEngine/Include/GameClient/View.h")
    #unify_file(Game.ZEROHOUR, "GameEngine/Source/GameClient/View.cpp", Game.CORE, "GameEngine/Source/GameClient/View.cpp")
    #unify_file(Game.ZEROHOUR, "GameEngineDevice/Include/W3DDevice/GameClient/W3DView.h", Game.CORE, "GameEngineDevice/Include/W3DDevice/GameClient/W3DView.h")
    #unify_file(Game.ZEROHOUR, "GameEngineDevice/Source/W3DDevice/GameClient/W3DView.cpp", Game.CORE, "GameEngineDevice/Source/W3DDevice/GameClient/W3DView.cpp")

    # WorldBuilder unification - files only in GeneralsMD
    #unify_move_file(Game.ZEROHOUR, "Tools/WorldBuilder/res/icon2.ico", Game.CORE, "Tools/WorldBuilder/res/icon2.ico")
    #unify_move_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/RulerOptions.h", Game.CORE, "Tools/WorldBuilder/include/RulerOptions.h")
    #unify_move_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/RulerTool.h", Game.CORE, "Tools/WorldBuilder/include/RulerTool.h")
    #unify_move_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/RulerOptions.cpp", Game.CORE, "Tools/WorldBuilder/src/RulerOptions.cpp")
    #unify_move_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/RulerTool.cpp", Game.CORE, "Tools/WorldBuilder/src/RulerTool.cpp")
    #unify_move_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/TeamObjectProperties.h", Game.CORE, "Tools/WorldBuilder/include/TeamObjectProperties.h")
    #unify_move_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/TeamObjectProperties.cpp", Game.CORE, "Tools/WorldBuilder/src/TeamObjectProperties.cpp")

    # WorldBuilder unification - files that exist in both
    #unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/AutoEdgeOutTool.cpp", Game.CORE, "Tools/WorldBuilder/src/AutoEdgeOutTool.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/BaseBuildProps.cpp", Game.CORE, "Tools/WorldBuilder/src/BaseBuildProps.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/BlendEdgeTool.cpp", Game.CORE, "Tools/WorldBuilder/src/BlendEdgeTool.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/BlendMaterial.cpp", Game.CORE, "Tools/WorldBuilder/src/BlendMaterial.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/BorderTool.cpp", Game.CORE, "Tools/WorldBuilder/src/BorderTool.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/BrushTool.cpp", Game.CORE, "Tools/WorldBuilder/src/BrushTool.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/BuildList.cpp", Game.CORE, "Tools/WorldBuilder/src/BuildList.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/BuildListTool.cpp", Game.CORE, "Tools/WorldBuilder/src/BuildListTool.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/CButtonShowColor.cpp", Game.CORE, "Tools/WorldBuilder/src/CButtonShowColor.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/CFixTeamOwnerDialog.cpp", Game.CORE, "Tools/WorldBuilder/src/CFixTeamOwnerDialog.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/CUndoable.cpp", Game.CORE, "Tools/WorldBuilder/src/CUndoable.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/CameraOptions.cpp", Game.CORE, "Tools/WorldBuilder/src/CameraOptions.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/CellWidth.cpp", Game.CORE, "Tools/WorldBuilder/src/CellWidth.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/ContourOptions.cpp", Game.CORE, "Tools/WorldBuilder/src/ContourOptions.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/DrawObject.cpp", Game.CORE, "Tools/WorldBuilder/src/DrawObject.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/EditAction.cpp", Game.CORE, "Tools/WorldBuilder/src/EditAction.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/EditCondition.cpp", Game.CORE, "Tools/WorldBuilder/src/EditCondition.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/EditCoordParameter.cpp", Game.CORE, "Tools/WorldBuilder/src/EditCoordParameter.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/EditGroup.cpp", Game.CORE, "Tools/WorldBuilder/src/EditGroup.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/EditObjectParameter.cpp", Game.CORE, "Tools/WorldBuilder/src/EditObjectParameter.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/EditParameter.cpp", Game.CORE, "Tools/WorldBuilder/src/EditParameter.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/EulaDialog.cpp", Game.CORE, "Tools/WorldBuilder/src/EulaDialog.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/ExportScriptsOptions.cpp", Game.CORE, "Tools/WorldBuilder/src/ExportScriptsOptions.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/EyedropperTool.cpp", Game.CORE, "Tools/WorldBuilder/src/EyedropperTool.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/FeatherOptions.cpp", Game.CORE, "Tools/WorldBuilder/src/FeatherOptions.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/FeatherTool.cpp", Game.CORE, "Tools/WorldBuilder/src/FeatherTool.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/FenceOptions.cpp", Game.CORE, "Tools/WorldBuilder/src/FenceOptions.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/FenceTool.cpp", Game.CORE, "Tools/WorldBuilder/src/FenceTool.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/FloodFillTool.cpp", Game.CORE, "Tools/WorldBuilder/src/FloodFillTool.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/GlobalLightOptions.cpp", Game.CORE, "Tools/WorldBuilder/src/GlobalLightOptions.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/GroveOptions.cpp", Game.CORE, "Tools/WorldBuilder/src/GroveOptions.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/GroveTool.cpp", Game.CORE, "Tools/WorldBuilder/src/GroveTool.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/HandScrollTool.cpp", Game.CORE, "Tools/WorldBuilder/src/HandScrollTool.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/ImpassableOptions.cpp", Game.CORE, "Tools/WorldBuilder/src/ImpassableOptions.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/LayersList.cpp", Game.CORE, "Tools/WorldBuilder/src/LayersList.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/LightOptions.cpp", Game.CORE, "Tools/WorldBuilder/src/LightOptions.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/MainFrm.cpp", Game.CORE, "Tools/WorldBuilder/src/MainFrm.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/MapPreview.cpp", Game.CORE, "Tools/WorldBuilder/src/MapPreview.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/MapSettings.cpp", Game.CORE, "Tools/WorldBuilder/src/MapSettings.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/MeshMoldOptions.cpp", Game.CORE, "Tools/WorldBuilder/src/MeshMoldOptions.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/MeshMoldTool.cpp", Game.CORE, "Tools/WorldBuilder/src/MeshMoldTool.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/MoundOptions.cpp", Game.CORE, "Tools/WorldBuilder/src/MoundOptions.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/MoundTool.cpp", Game.CORE, "Tools/WorldBuilder/src/MoundTool.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/MyToolbar.cpp", Game.CORE, "Tools/WorldBuilder/src/MyToolbar.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/NewHeightMap.cpp", Game.CORE, "Tools/WorldBuilder/src/NewHeightMap.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/ObjectOptions.cpp", Game.CORE, "Tools/WorldBuilder/src/ObjectOptions.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/ObjectPreview.cpp", Game.CORE, "Tools/WorldBuilder/src/ObjectPreview.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/ObjectTool.cpp", Game.CORE, "Tools/WorldBuilder/src/ObjectTool.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/OpenMap.cpp", Game.CORE, "Tools/WorldBuilder/src/OpenMap.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/OptionsPanel.cpp", Game.CORE, "Tools/WorldBuilder/src/OptionsPanel.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/PickUnitDialog.cpp", Game.CORE, "Tools/WorldBuilder/src/PickUnitDialog.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/PointerTool.cpp", Game.CORE, "Tools/WorldBuilder/src/PointerTool.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/PolygonTool.cpp", Game.CORE, "Tools/WorldBuilder/src/PolygonTool.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/RampOptions.cpp", Game.CORE, "Tools/WorldBuilder/src/RampOptions.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/RampTool.cpp", Game.CORE, "Tools/WorldBuilder/src/RampTool.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/RoadOptions.cpp", Game.CORE, "Tools/WorldBuilder/src/RoadOptions.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/RoadTool.cpp", Game.CORE, "Tools/WorldBuilder/src/RoadTool.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/SaveMap.cpp", Game.CORE, "Tools/WorldBuilder/src/SaveMap.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/ScorchOptions.cpp", Game.CORE, "Tools/WorldBuilder/src/ScorchOptions.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/ScorchTool.cpp", Game.CORE, "Tools/WorldBuilder/src/ScorchTool.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/ScriptActionsFalse.cpp", Game.CORE, "Tools/WorldBuilder/src/ScriptActionsFalse.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/ScriptActionsTrue.cpp", Game.CORE, "Tools/WorldBuilder/src/ScriptActionsTrue.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/ScriptConditions.cpp", Game.CORE, "Tools/WorldBuilder/src/ScriptConditions.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/ScriptDialog.cpp", Game.CORE, "Tools/WorldBuilder/src/ScriptDialog.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/ScriptProperties.cpp", Game.CORE, "Tools/WorldBuilder/src/ScriptProperties.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/SelectMacrotexture.cpp", Game.CORE, "Tools/WorldBuilder/src/SelectMacrotexture.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/ShadowOptions.cpp", Game.CORE, "Tools/WorldBuilder/src/ShadowOptions.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/SplashScreen.cpp", Game.CORE, "Tools/WorldBuilder/src/SplashScreen.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/StdAfx.cpp", Game.CORE, "Tools/WorldBuilder/src/StdAfx.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/TeamBehavior.cpp", Game.CORE, "Tools/WorldBuilder/src/TeamBehavior.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/TeamGeneric.cpp", Game.CORE, "Tools/WorldBuilder/src/TeamGeneric.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/TeamIdentity.cpp", Game.CORE, "Tools/WorldBuilder/src/TeamIdentity.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/TeamReinforcement.cpp", Game.CORE, "Tools/WorldBuilder/src/TeamReinforcement.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/TerrainMaterial.cpp", Game.CORE, "Tools/WorldBuilder/src/TerrainMaterial.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/TerrainModal.cpp", Game.CORE, "Tools/WorldBuilder/src/TerrainModal.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/TerrainSwatches.cpp", Game.CORE, "Tools/WorldBuilder/src/TerrainSwatches.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/TileTool.cpp", Game.CORE, "Tools/WorldBuilder/src/TileTool.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/Tool.cpp", Game.CORE, "Tools/WorldBuilder/src/Tool.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/WBFrameWnd.cpp", Game.CORE, "Tools/WorldBuilder/src/WBFrameWnd.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/WBHeightMap.cpp", Game.CORE, "Tools/WorldBuilder/src/WBHeightMap.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/WBPopupSlider.cpp", Game.CORE, "Tools/WorldBuilder/src/WBPopupSlider.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/WHeightMapEdit.cpp", Game.CORE, "Tools/WorldBuilder/src/WHeightMapEdit.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/WaterOptions.cpp", Game.CORE, "Tools/WorldBuilder/src/WaterOptions.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/WaterTool.cpp", Game.CORE, "Tools/WorldBuilder/src/WaterTool.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/WaypointOptions.cpp", Game.CORE, "Tools/WorldBuilder/src/WaypointOptions.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/WaypointTool.cpp", Game.CORE, "Tools/WorldBuilder/src/WaypointTool.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/WorldBuilder.cpp", Game.CORE, "Tools/WorldBuilder/src/WorldBuilder.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/WorldBuilderDoc.cpp", Game.CORE, "Tools/WorldBuilder/src/WorldBuilderDoc.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/WorldBuilderView.cpp", Game.CORE, "Tools/WorldBuilder/src/WorldBuilderView.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/addplayerdialog.cpp", Game.CORE, "Tools/WorldBuilder/src/addplayerdialog.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/brushoptions.cpp", Game.CORE, "Tools/WorldBuilder/src/brushoptions.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/mapobjectprops.cpp", Game.CORE, "Tools/WorldBuilder/src/mapobjectprops.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/playerlistdlg.cpp", Game.CORE, "Tools/WorldBuilder/src/playerlistdlg.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/propedit.cpp", Game.CORE, "Tools/WorldBuilder/src/propedit.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/teamsdialog.cpp", Game.CORE, "Tools/WorldBuilder/src/teamsdialog.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/wbview.cpp", Game.CORE, "Tools/WorldBuilder/src/wbview.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/src/wbview3d.cpp", Game.CORE, "Tools/WorldBuilder/src/wbview3d.cpp")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/AutoEdgeOutTool.h", Game.CORE, "Tools/WorldBuilder/include/AutoEdgeOutTool.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/BaseBuildProps.h", Game.CORE, "Tools/WorldBuilder/include/BaseBuildProps.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/BlendEdgeTool.h", Game.CORE, "Tools/WorldBuilder/include/BlendEdgeTool.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/BlendMaterial.h", Game.CORE, "Tools/WorldBuilder/include/BlendMaterial.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/BorderTool.h", Game.CORE, "Tools/WorldBuilder/include/BorderTool.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/BrushTool.h", Game.CORE, "Tools/WorldBuilder/include/BrushTool.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/BuildList.h", Game.CORE, "Tools/WorldBuilder/include/BuildList.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/BuildListTool.h", Game.CORE, "Tools/WorldBuilder/include/BuildListTool.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/CButtonShowColor.h", Game.CORE, "Tools/WorldBuilder/include/CButtonShowColor.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/CFixTeamOwnerDialog.h", Game.CORE, "Tools/WorldBuilder/include/CFixTeamOwnerDialog.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/CUndoable.h", Game.CORE, "Tools/WorldBuilder/include/CUndoable.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/CameraOptions.h", Game.CORE, "Tools/WorldBuilder/include/CameraOptions.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/CellWidth.h", Game.CORE, "Tools/WorldBuilder/include/CellWidth.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/ContourOptions.h", Game.CORE, "Tools/WorldBuilder/include/ContourOptions.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/DrawObject.h", Game.CORE, "Tools/WorldBuilder/include/DrawObject.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/EditAction.h", Game.CORE, "Tools/WorldBuilder/include/EditAction.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/EditCondition.h", Game.CORE, "Tools/WorldBuilder/include/EditCondition.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/EditCoordParameter.h", Game.CORE, "Tools/WorldBuilder/include/EditCoordParameter.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/EditGroup.h", Game.CORE, "Tools/WorldBuilder/include/EditGroup.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/EditObjectParameter.h", Game.CORE, "Tools/WorldBuilder/include/EditObjectParameter.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/EditParameter.h", Game.CORE, "Tools/WorldBuilder/include/EditParameter.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/ExportScriptsOptions.h", Game.CORE, "Tools/WorldBuilder/include/ExportScriptsOptions.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/EyedropperTool.h", Game.CORE, "Tools/WorldBuilder/include/EyedropperTool.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/FeatherOptions.h", Game.CORE, "Tools/WorldBuilder/include/FeatherOptions.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/FeatherTool.h", Game.CORE, "Tools/WorldBuilder/include/FeatherTool.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/FenceOptions.h", Game.CORE, "Tools/WorldBuilder/include/FenceOptions.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/FenceTool.h", Game.CORE, "Tools/WorldBuilder/include/FenceTool.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/FloodFillTool.h", Game.CORE, "Tools/WorldBuilder/include/FloodFillTool.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/GlobalLightOptions.h", Game.CORE, "Tools/WorldBuilder/include/GlobalLightOptions.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/GroveOptions.h", Game.CORE, "Tools/WorldBuilder/include/GroveOptions.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/GroveTool.h", Game.CORE, "Tools/WorldBuilder/include/GroveTool.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/HandScrollTool.h", Game.CORE, "Tools/WorldBuilder/include/HandScrollTool.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/ImpassableOptions.h", Game.CORE, "Tools/WorldBuilder/include/ImpassableOptions.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/LayersList.h", Game.CORE, "Tools/WorldBuilder/include/LayersList.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/LightOptions.h", Game.CORE, "Tools/WorldBuilder/include/LightOptions.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/MainFrm.h", Game.CORE, "Tools/WorldBuilder/include/MainFrm.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/MapPreview.h", Game.CORE, "Tools/WorldBuilder/include/MapPreview.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/MapSettings.h", Game.CORE, "Tools/WorldBuilder/include/MapSettings.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/MeshMoldOptions.h", Game.CORE, "Tools/WorldBuilder/include/MeshMoldOptions.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/MeshMoldTool.h", Game.CORE, "Tools/WorldBuilder/include/MeshMoldTool.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/MoundOptions.h", Game.CORE, "Tools/WorldBuilder/include/MoundOptions.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/MoundTool.h", Game.CORE, "Tools/WorldBuilder/include/MoundTool.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/MyToolbar.h", Game.CORE, "Tools/WorldBuilder/include/MyToolbar.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/NewHeightMap.h", Game.CORE, "Tools/WorldBuilder/include/NewHeightMap.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/ObjectOptions.h", Game.CORE, "Tools/WorldBuilder/include/ObjectOptions.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/ObjectPreview.h", Game.CORE, "Tools/WorldBuilder/include/ObjectPreview.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/ObjectTool.h", Game.CORE, "Tools/WorldBuilder/include/ObjectTool.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/OpenMap.h", Game.CORE, "Tools/WorldBuilder/include/OpenMap.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/OptionsPanel.h", Game.CORE, "Tools/WorldBuilder/include/OptionsPanel.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/PickUnitDialog.h", Game.CORE, "Tools/WorldBuilder/include/PickUnitDialog.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/PointerTool.h", Game.CORE, "Tools/WorldBuilder/include/PointerTool.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/PolygonTool.h", Game.CORE, "Tools/WorldBuilder/include/PolygonTool.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/RampOptions.h", Game.CORE, "Tools/WorldBuilder/include/RampOptions.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/RampTool.h", Game.CORE, "Tools/WorldBuilder/include/RampTool.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/RoadOptions.h", Game.CORE, "Tools/WorldBuilder/include/RoadOptions.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/RoadTool.h", Game.CORE, "Tools/WorldBuilder/include/RoadTool.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/SaveMap.h", Game.CORE, "Tools/WorldBuilder/include/SaveMap.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/ScorchOptions.h", Game.CORE, "Tools/WorldBuilder/include/ScorchOptions.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/ScorchTool.h", Game.CORE, "Tools/WorldBuilder/include/ScorchTool.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/ScriptActionsFalse.h", Game.CORE, "Tools/WorldBuilder/include/ScriptActionsFalse.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/ScriptActionsTrue.h", Game.CORE, "Tools/WorldBuilder/include/ScriptActionsTrue.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/ScriptConditions.h", Game.CORE, "Tools/WorldBuilder/include/ScriptConditions.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/ScriptDialog.h", Game.CORE, "Tools/WorldBuilder/include/ScriptDialog.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/ScriptProperties.h", Game.CORE, "Tools/WorldBuilder/include/ScriptProperties.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/SelectMacrotexture.h", Game.CORE, "Tools/WorldBuilder/include/SelectMacrotexture.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/ShadowOptions.h", Game.CORE, "Tools/WorldBuilder/include/ShadowOptions.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/SplashScreen.h", Game.CORE, "Tools/WorldBuilder/include/SplashScreen.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/StdAfx.h", Game.CORE, "Tools/WorldBuilder/include/StdAfx.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/TeamBehavior.h", Game.CORE, "Tools/WorldBuilder/include/TeamBehavior.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/TeamGeneric.h", Game.CORE, "Tools/WorldBuilder/include/TeamGeneric.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/TeamIdentity.h", Game.CORE, "Tools/WorldBuilder/include/TeamIdentity.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/TeamReinforcement.h", Game.CORE, "Tools/WorldBuilder/include/TeamReinforcement.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/TerrainMaterial.h", Game.CORE, "Tools/WorldBuilder/include/TerrainMaterial.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/TerrainModal.h", Game.CORE, "Tools/WorldBuilder/include/TerrainModal.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/TerrainSwatches.h", Game.CORE, "Tools/WorldBuilder/include/TerrainSwatches.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/TileTool.h", Game.CORE, "Tools/WorldBuilder/include/TileTool.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/Tool.h", Game.CORE, "Tools/WorldBuilder/include/Tool.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/WBFrameWnd.h", Game.CORE, "Tools/WorldBuilder/include/WBFrameWnd.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/WBHeightMap.h", Game.CORE, "Tools/WorldBuilder/include/WBHeightMap.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/WBPopupSlider.h", Game.CORE, "Tools/WorldBuilder/include/WBPopupSlider.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/WHeightMapEdit.h", Game.CORE, "Tools/WorldBuilder/include/WHeightMapEdit.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/WaterOptions.h", Game.CORE, "Tools/WorldBuilder/include/WaterOptions.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/WaterTool.h", Game.CORE, "Tools/WorldBuilder/include/WaterTool.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/WaypointOptions.h", Game.CORE, "Tools/WorldBuilder/include/WaypointOptions.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/WaypointTool.h", Game.CORE, "Tools/WorldBuilder/include/WaypointTool.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/WorldBuilder.h", Game.CORE, "Tools/WorldBuilder/include/WorldBuilder.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/WorldBuilderDoc.h", Game.CORE, "Tools/WorldBuilder/include/WorldBuilderDoc.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/WorldBuilderView.h", Game.CORE, "Tools/WorldBuilder/include/WorldBuilderView.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/addplayerdialog.h", Game.CORE, "Tools/WorldBuilder/include/addplayerdialog.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/brushoptions.h", Game.CORE, "Tools/WorldBuilder/include/brushoptions.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/euladialog.h", Game.CORE, "Tools/WorldBuilder/include/euladialog.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/mapobjectprops.h", Game.CORE, "Tools/WorldBuilder/include/mapobjectprops.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/playerlistdlg.h", Game.CORE, "Tools/WorldBuilder/include/playerlistdlg.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/propedit.h", Game.CORE, "Tools/WorldBuilder/include/propedit.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/teamsdialog.h", Game.CORE, "Tools/WorldBuilder/include/teamsdialog.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/wbview.h", Game.CORE, "Tools/WorldBuilder/include/wbview.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/include/wbview3d.h", Game.CORE, "Tools/WorldBuilder/include/wbview3d.h")
#    unify_file(Game.ZEROHOUR, "Tools/WorldBuilder/res/resource.h", Game.CORE, "Tools/WorldBuilder/res/resource.h")

    return


if __name__ == "__main__":
    main()
