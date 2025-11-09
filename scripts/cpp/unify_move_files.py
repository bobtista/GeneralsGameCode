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
    # GameNetwork Headers
    unify_file(Game.ZEROHOUR, "GameEngine/Include/GameNetwork/Connection.h", Game.CORE, "GameEngine/Include/GameNetwork/Connection.h")
    unify_file(Game.ZEROHOUR, "GameEngine/Include/GameNetwork/ConnectionManager.h", Game.CORE, "GameEngine/Include/GameNetwork/ConnectionManager.h")
    unify_file(Game.ZEROHOUR, "GameEngine/Include/GameNetwork/DisconnectManager.h", Game.CORE, "GameEngine/Include/GameNetwork/DisconnectManager.h")
    unify_file(Game.ZEROHOUR, "GameEngine/Include/GameNetwork/DownloadManager.h", Game.CORE, "GameEngine/Include/GameNetwork/DownloadManager.h")
    unify_file(Game.ZEROHOUR, "GameEngine/Include/GameNetwork/FileTransfer.h", Game.CORE, "GameEngine/Include/GameNetwork/FileTransfer.h")
    unify_file(Game.ZEROHOUR, "GameEngine/Include/GameNetwork/FirewallHelper.h", Game.CORE, "GameEngine/Include/GameNetwork/FirewallHelper.h")
    unify_file(Game.ZEROHOUR, "GameEngine/Include/GameNetwork/FrameData.h", Game.CORE, "GameEngine/Include/GameNetwork/FrameData.h")
    unify_file(Game.ZEROHOUR, "GameEngine/Include/GameNetwork/FrameDataManager.h", Game.CORE, "GameEngine/Include/GameNetwork/FrameDataManager.h")
    unify_file(Game.ZEROHOUR, "GameEngine/Include/GameNetwork/FrameMetrics.h", Game.CORE, "GameEngine/Include/GameNetwork/FrameMetrics.h")
    unify_file(Game.ZEROHOUR, "GameEngine/Include/GameNetwork/GameInfo.h", Game.CORE, "GameEngine/Include/GameNetwork/GameInfo.h")
    unify_file(Game.ZEROHOUR, "GameEngine/Include/GameNetwork/GameMessageParser.h", Game.CORE, "GameEngine/Include/GameNetwork/GameMessageParser.h")
    unify_file(Game.ZEROHOUR, "GameEngine/Include/GameNetwork/GameSpyChat.h", Game.CORE, "GameEngine/Include/GameNetwork/GameSpyChat.h")
    unify_file(Game.ZEROHOUR, "GameEngine/Include/GameNetwork/GameSpyGP.h", Game.CORE, "GameEngine/Include/GameNetwork/GameSpyGP.h")
    unify_file(Game.ZEROHOUR, "GameEngine/Include/GameNetwork/GameSpyGameInfo.h", Game.CORE, "GameEngine/Include/GameNetwork/GameSpyGameInfo.h")
    unify_file(Game.ZEROHOUR, "GameEngine/Include/GameNetwork/GameSpyOverlay.h", Game.CORE, "GameEngine/Include/GameNetwork/GameSpyOverlay.h")
    unify_file(Game.ZEROHOUR, "GameEngine/Include/GameNetwork/GameSpyThread.h", Game.CORE, "GameEngine/Include/GameNetwork/GameSpyThread.h")
    unify_file(Game.ZEROHOUR, "GameEngine/Include/GameNetwork/IPEnumeration.h", Game.CORE, "GameEngine/Include/GameNetwork/IPEnumeration.h")
    unify_file(Game.ZEROHOUR, "GameEngine/Include/GameNetwork/LANAPI.h", Game.CORE, "GameEngine/Include/GameNetwork/LANAPI.h")
    unify_file(Game.ZEROHOUR, "GameEngine/Include/GameNetwork/LANAPICallbacks.h", Game.CORE, "GameEngine/Include/GameNetwork/LANAPICallbacks.h")
    unify_file(Game.ZEROHOUR, "GameEngine/Include/GameNetwork/LANGameInfo.h", Game.CORE, "GameEngine/Include/GameNetwork/LANGameInfo.h")
    unify_file(Game.ZEROHOUR, "GameEngine/Include/GameNetwork/LANPlayer.h", Game.CORE, "GameEngine/Include/GameNetwork/LANPlayer.h")
    unify_file(Game.ZEROHOUR, "GameEngine/Include/GameNetwork/NAT.h", Game.CORE, "GameEngine/Include/GameNetwork/NAT.h")
    unify_file(Game.ZEROHOUR, "GameEngine/Include/GameNetwork/NetCommandList.h", Game.CORE, "GameEngine/Include/GameNetwork/NetCommandList.h")
    unify_file(Game.ZEROHOUR, "GameEngine/Include/GameNetwork/NetCommandMsg.h", Game.CORE, "GameEngine/Include/GameNetwork/NetCommandMsg.h")
    unify_file(Game.ZEROHOUR, "GameEngine/Include/GameNetwork/NetCommandRef.h", Game.CORE, "GameEngine/Include/GameNetwork/NetCommandRef.h")
    unify_file(Game.ZEROHOUR, "GameEngine/Include/GameNetwork/NetCommandWrapperList.h", Game.CORE, "GameEngine/Include/GameNetwork/NetCommandWrapperList.h")
    unify_file(Game.ZEROHOUR, "GameEngine/Include/GameNetwork/NetPacket.h", Game.CORE, "GameEngine/Include/GameNetwork/NetPacket.h")
    unify_file(Game.ZEROHOUR, "GameEngine/Include/GameNetwork/NetworkDefs.h", Game.CORE, "GameEngine/Include/GameNetwork/NetworkDefs.h")
    unify_file(Game.ZEROHOUR, "GameEngine/Include/GameNetwork/NetworkInterface.h", Game.CORE, "GameEngine/Include/GameNetwork/NetworkInterface.h")
    unify_file(Game.ZEROHOUR, "GameEngine/Include/GameNetwork/RankPointValue.h", Game.CORE, "GameEngine/Include/GameNetwork/RankPointValue.h")
    unify_file(Game.ZEROHOUR, "GameEngine/Include/GameNetwork/Transport.h", Game.CORE, "GameEngine/Include/GameNetwork/Transport.h")
    unify_file(Game.ZEROHOUR, "GameEngine/Include/GameNetwork/User.h", Game.CORE, "GameEngine/Include/GameNetwork/User.h")
    unify_file(Game.ZEROHOUR, "GameEngine/Include/GameNetwork/WOLBrowser/FEBDispatch.h", Game.CORE, "GameEngine/Include/GameNetwork/WOLBrowser/FEBDispatch.h")
    unify_file(Game.ZEROHOUR, "GameEngine/Include/GameNetwork/WOLBrowser/WebBrowser.h", Game.CORE, "GameEngine/Include/GameNetwork/WOLBrowser/WebBrowser.h")
    unify_file(Game.ZEROHOUR, "GameEngine/Include/GameNetwork/networkutil.h", Game.CORE, "GameEngine/Include/GameNetwork/networkutil.h")
    unify_file(Game.ZEROHOUR, "GameEngine/Include/GameNetwork/udp.h", Game.CORE, "GameEngine/Include/GameNetwork/udp.h")
    
    # GameSpy Headers
    unify_file(Game.ZEROHOUR, "GameEngine/Include/GameNetwork/GameSpy/BuddyDefs.h", Game.CORE, "GameEngine/Include/GameNetwork/GameSpy/BuddyDefs.h")
    unify_file(Game.ZEROHOUR, "GameEngine/Include/GameNetwork/GameSpy/BuddyThread.h", Game.CORE, "GameEngine/Include/GameNetwork/GameSpy/BuddyThread.h")
    unify_file(Game.ZEROHOUR, "GameEngine/Include/GameNetwork/GameSpy/GSConfig.h", Game.CORE, "GameEngine/Include/GameNetwork/GameSpy/GSConfig.h")
    unify_file(Game.ZEROHOUR, "GameEngine/Include/GameNetwork/GameSpy/GameResultsThread.h", Game.CORE, "GameEngine/Include/GameNetwork/GameSpy/GameResultsThread.h")
    unify_file(Game.ZEROHOUR, "GameEngine/Include/GameNetwork/GameSpy/LadderDefs.h", Game.CORE, "GameEngine/Include/GameNetwork/GameSpy/LadderDefs.h")
    unify_file(Game.ZEROHOUR, "GameEngine/Include/GameNetwork/GameSpy/LobbyUtils.h", Game.CORE, "GameEngine/Include/GameNetwork/GameSpy/LobbyUtils.h")
    unify_file(Game.ZEROHOUR, "GameEngine/Include/GameNetwork/GameSpy/MainMenuUtils.h", Game.CORE, "GameEngine/Include/GameNetwork/GameSpy/MainMenuUtils.h")
    unify_file(Game.ZEROHOUR, "GameEngine/Include/GameNetwork/GameSpy/PeerDefs.h", Game.CORE, "GameEngine/Include/GameNetwork/GameSpy/PeerDefs.h")
    unify_file(Game.ZEROHOUR, "GameEngine/Include/GameNetwork/GameSpy/PeerDefsImplementation.h", Game.CORE, "GameEngine/Include/GameNetwork/GameSpy/PeerDefsImplementation.h")
    unify_file(Game.ZEROHOUR, "GameEngine/Include/GameNetwork/GameSpy/PeerThread.h", Game.CORE, "GameEngine/Include/GameNetwork/GameSpy/PeerThread.h")
    unify_file(Game.ZEROHOUR, "GameEngine/Include/GameNetwork/GameSpy/PersistentStorageDefs.h", Game.CORE, "GameEngine/Include/GameNetwork/GameSpy/PersistentStorageDefs.h")
    unify_file(Game.ZEROHOUR, "GameEngine/Include/GameNetwork/GameSpy/PersistentStorageThread.h", Game.CORE, "GameEngine/Include/GameNetwork/GameSpy/PersistentStorageThread.h")
    unify_file(Game.ZEROHOUR, "GameEngine/Include/GameNetwork/GameSpy/PingThread.h", Game.CORE, "GameEngine/Include/GameNetwork/GameSpy/PingThread.h")
    unify_file(Game.ZEROHOUR, "GameEngine/Include/GameNetwork/GameSpy/StagingRoomGameInfo.h", Game.CORE, "GameEngine/Include/GameNetwork/GameSpy/StagingRoomGameInfo.h")
    unify_file(Game.ZEROHOUR, "GameEngine/Include/GameNetwork/GameSpy/ThreadUtils.h", Game.CORE, "GameEngine/Include/GameNetwork/GameSpy/ThreadUtils.h")

    # GameNetwork Source
    unify_file(Game.ZEROHOUR, "GameEngine/Source/GameNetwork/Connection.cpp", Game.CORE, "GameEngine/Source/GameNetwork/Connection.cpp")
    unify_file(Game.ZEROHOUR, "GameEngine/Source/GameNetwork/ConnectionManager.cpp", Game.CORE, "GameEngine/Source/GameNetwork/ConnectionManager.cpp")
    unify_file(Game.ZEROHOUR, "GameEngine/Source/GameNetwork/DisconnectManager.cpp", Game.CORE, "GameEngine/Source/GameNetwork/DisconnectManager.cpp")
    unify_file(Game.ZEROHOUR, "GameEngine/Source/GameNetwork/DownloadManager.cpp", Game.CORE, "GameEngine/Source/GameNetwork/DownloadManager.cpp")
    unify_file(Game.ZEROHOUR, "GameEngine/Source/GameNetwork/FileTransfer.cpp", Game.CORE, "GameEngine/Source/GameNetwork/FileTransfer.cpp")
    unify_file(Game.ZEROHOUR, "GameEngine/Source/GameNetwork/FirewallHelper.cpp", Game.CORE, "GameEngine/Source/GameNetwork/FirewallHelper.cpp")
    unify_file(Game.ZEROHOUR, "GameEngine/Source/GameNetwork/FrameData.cpp", Game.CORE, "GameEngine/Source/GameNetwork/FrameData.cpp")
    unify_file(Game.ZEROHOUR, "GameEngine/Source/GameNetwork/FrameDataManager.cpp", Game.CORE, "GameEngine/Source/GameNetwork/FrameDataManager.cpp")
    unify_file(Game.ZEROHOUR, "GameEngine/Source/GameNetwork/FrameMetrics.cpp", Game.CORE, "GameEngine/Source/GameNetwork/FrameMetrics.cpp")
    unify_file(Game.ZEROHOUR, "GameEngine/Source/GameNetwork/GameInfo.cpp", Game.CORE, "GameEngine/Source/GameNetwork/GameInfo.cpp")
    unify_file(Game.ZEROHOUR, "GameEngine/Source/GameNetwork/GameMessageParser.cpp", Game.CORE, "GameEngine/Source/GameNetwork/GameMessageParser.cpp")
    unify_file(Game.ZEROHOUR, "GameEngine/Source/GameNetwork/GameSpyChat.cpp", Game.CORE, "GameEngine/Source/GameNetwork/GameSpyChat.cpp")
    unify_file(Game.ZEROHOUR, "GameEngine/Source/GameNetwork/GameSpyGP.cpp", Game.CORE, "GameEngine/Source/GameNetwork/GameSpyGP.cpp")
    unify_file(Game.ZEROHOUR, "GameEngine/Source/GameNetwork/GameSpyGameInfo.cpp", Game.CORE, "GameEngine/Source/GameNetwork/GameSpyGameInfo.cpp")
    unify_file(Game.ZEROHOUR, "GameEngine/Source/GameNetwork/GameSpyOverlay.cpp", Game.CORE, "GameEngine/Source/GameNetwork/GameSpyOverlay.cpp")
    unify_file(Game.ZEROHOUR, "GameEngine/Source/GameNetwork/IPEnumeration.cpp", Game.CORE, "GameEngine/Source/GameNetwork/IPEnumeration.cpp")
    unify_file(Game.ZEROHOUR, "GameEngine/Source/GameNetwork/LANAPI.cpp", Game.CORE, "GameEngine/Source/GameNetwork/LANAPI.cpp")
    unify_file(Game.ZEROHOUR, "GameEngine/Source/GameNetwork/LANAPICallbacks.cpp", Game.CORE, "GameEngine/Source/GameNetwork/LANAPICallbacks.cpp")
    unify_file(Game.ZEROHOUR, "GameEngine/Source/GameNetwork/LANAPIhandlers.cpp", Game.CORE, "GameEngine/Source/GameNetwork/LANAPIhandlers.cpp")
    unify_file(Game.ZEROHOUR, "GameEngine/Source/GameNetwork/LANGameInfo.cpp", Game.CORE, "GameEngine/Source/GameNetwork/LANGameInfo.cpp")
    unify_file(Game.ZEROHOUR, "GameEngine/Source/GameNetwork/NAT.cpp", Game.CORE, "GameEngine/Source/GameNetwork/NAT.cpp")
    unify_file(Game.ZEROHOUR, "GameEngine/Source/GameNetwork/NetCommandList.cpp", Game.CORE, "GameEngine/Source/GameNetwork/NetCommandList.cpp")
    unify_file(Game.ZEROHOUR, "GameEngine/Source/GameNetwork/NetCommandMsg.cpp", Game.CORE, "GameEngine/Source/GameNetwork/NetCommandMsg.cpp")
    unify_file(Game.ZEROHOUR, "GameEngine/Source/GameNetwork/NetCommandRef.cpp", Game.CORE, "GameEngine/Source/GameNetwork/NetCommandRef.cpp")
    unify_file(Game.ZEROHOUR, "GameEngine/Source/GameNetwork/NetCommandWrapperList.cpp", Game.CORE, "GameEngine/Source/GameNetwork/NetCommandWrapperList.cpp")
    unify_file(Game.ZEROHOUR, "GameEngine/Source/GameNetwork/NetMessageStream.cpp", Game.CORE, "GameEngine/Source/GameNetwork/NetMessageStream.cpp")
    unify_file(Game.ZEROHOUR, "GameEngine/Source/GameNetwork/NetPacket.cpp", Game.CORE, "GameEngine/Source/GameNetwork/NetPacket.cpp")
    unify_file(Game.ZEROHOUR, "GameEngine/Source/GameNetwork/Network.cpp", Game.CORE, "GameEngine/Source/GameNetwork/Network.cpp")
    unify_file(Game.ZEROHOUR, "GameEngine/Source/GameNetwork/NetworkUtil.cpp", Game.CORE, "GameEngine/Source/GameNetwork/NetworkUtil.cpp")
    unify_file(Game.ZEROHOUR, "GameEngine/Source/GameNetwork/Transport.cpp", Game.CORE, "GameEngine/Source/GameNetwork/Transport.cpp")
    unify_file(Game.ZEROHOUR, "GameEngine/Source/GameNetwork/User.cpp", Game.CORE, "GameEngine/Source/GameNetwork/User.cpp")
    unify_file(Game.ZEROHOUR, "GameEngine/Source/GameNetwork/WOLBrowser/WebBrowser.cpp", Game.CORE, "GameEngine/Source/GameNetwork/WOLBrowser/WebBrowser.cpp")
    unify_file(Game.ZEROHOUR, "GameEngine/Source/GameNetwork/udp.cpp", Game.CORE, "GameEngine/Source/GameNetwork/udp.cpp")

    # GameSpy Source files
    unify_file(Game.ZEROHOUR, "GameEngine/Source/GameNetwork/GameSpy/Chat.cpp", Game.CORE, "GameEngine/Source/GameNetwork/GameSpy/Chat.cpp")
    unify_file(Game.ZEROHOUR, "GameEngine/Source/GameNetwork/GameSpy/GSConfig.cpp", Game.CORE, "GameEngine/Source/GameNetwork/GameSpy/GSConfig.cpp")
    unify_file(Game.ZEROHOUR, "GameEngine/Source/GameNetwork/GameSpy/LadderDefs.cpp", Game.CORE, "GameEngine/Source/GameNetwork/GameSpy/LadderDefs.cpp")
    unify_file(Game.ZEROHOUR, "GameEngine/Source/GameNetwork/GameSpy/LobbyUtils.cpp", Game.CORE, "GameEngine/Source/GameNetwork/GameSpy/LobbyUtils.cpp")
    unify_file(Game.ZEROHOUR, "GameEngine/Source/GameNetwork/GameSpy/MainMenuUtils.cpp", Game.CORE, "GameEngine/Source/GameNetwork/GameSpy/MainMenuUtils.cpp")
    unify_file(Game.ZEROHOUR, "GameEngine/Source/GameNetwork/GameSpy/PeerDefs.cpp", Game.CORE, "GameEngine/Source/GameNetwork/GameSpy/PeerDefs.cpp")
    unify_file(Game.ZEROHOUR, "GameEngine/Source/GameNetwork/GameSpy/StagingRoomGameInfo.cpp", Game.CORE, "GameEngine/Source/GameNetwork/GameSpy/StagingRoomGameInfo.cpp")

    # GameSpy Thread files
    unify_file(Game.ZEROHOUR, "GameEngine/Source/GameNetwork/GameSpy/Thread/BuddyThread.cpp", Game.CORE, "GameEngine/Source/GameNetwork/GameSpy/Thread/BuddyThread.cpp")
    unify_file(Game.ZEROHOUR, "GameEngine/Source/GameNetwork/GameSpy/Thread/GameResultsThread.cpp", Game.CORE, "GameEngine/Source/GameNetwork/GameSpy/Thread/GameResultsThread.cpp")
    unify_file(Game.ZEROHOUR, "GameEngine/Source/GameNetwork/GameSpy/Thread/PeerThread.cpp", Game.CORE, "GameEngine/Source/GameNetwork/GameSpy/Thread/PeerThread.cpp")
    unify_file(Game.ZEROHOUR, "GameEngine/Source/GameNetwork/GameSpy/Thread/PersistentStorageThread.cpp", Game.CORE, "GameEngine/Source/GameNetwork/GameSpy/Thread/PersistentStorageThread.cpp")
    unify_file(Game.ZEROHOUR, "GameEngine/Source/GameNetwork/GameSpy/Thread/PingThread.cpp", Game.CORE, "GameEngine/Source/GameNetwork/GameSpy/Thread/PingThread.cpp")
    unify_file(Game.ZEROHOUR, "GameEngine/Source/GameNetwork/GameSpy/Thread/ThreadUtils.cpp", Game.CORE, "GameEngine/Source/GameNetwork/GameSpy/Thread/ThreadUtils.cpp")

    return


if __name__ == "__main__":
    main()
