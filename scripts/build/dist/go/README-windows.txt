GeneralsOnline bgfx (Windows, 32-bit). Unofficial build, not made by the GeneralsOnline team.

1. Extract the GeneralsOnline-bgfx folder into your Zero Hour game folder.
   You should end up with <Zero Hour folder>\GeneralsOnline-bgfx\GeneralsOnlineZH.exe.
   Do not copy these files into the main game folder. The DLLs would overwrite the
   64-bit ones other builds use.
2. Run "Play GeneralsOnline bgfx.bat" inside that folder. Make a desktop shortcut to it if you like.
3. Log in with your GeneralsOnline account.

Render settings are read from Data\INI\Bgfx.ini in the main game folder.
Keep Data\INI\GameData.ini stock, or lobbies will reject you for modified INI files.

If it says a Visual C++ runtime is missing, install the x86 version:
https://aka.ms/vs/17/release/vc_redist.x86.exe

This build does not auto update. Get the newest zip from:
https://github.com/bobtista/GeneralsGameCode/releases/tag/latest-go-bgfx
