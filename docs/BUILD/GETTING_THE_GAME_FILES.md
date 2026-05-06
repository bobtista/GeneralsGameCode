# Getting the Game Files

GeneralsGameCode is the open-source game engine. To play, you also need the retail game data files (maps, models, textures, audio) from Command & Conquer: Generals and Zero Hour.

Steam does not offer a macOS or Linux download, so you need to obtain the Windows game files and copy them to your runtime directory.

## Option 1 — Copy From a Windows Installation (Easiest)

If you have Generals Zero Hour installed on a Windows PC (Steam, Origin, or disc):

1. Locate your installation directories:
   - **Steam:** `C:\Program Files (x86)\Steam\steamapps\common\Command and Conquer Generals Zero Hour\`
   - **Origin/EA App:** `C:\Program Files (x86)\Origin Games\Command and Conquer Generals Zero Hour\`

2. Copy all `.big` files to your runtime directory (`~/TheSuperHackers/GeneralsZH/` on macOS).

3. Transfer via USB drive, network share, or cloud storage.

## Option 2 — SteamCMD (No Windows PC Needed)

SteamCMD is Valve's headless Steam client. It runs on macOS and Linux and can download Windows game files if you own the game on Steam.

**Install SteamCMD:**
```bash
# macOS
brew install steamcmd

# Linux (Debian/Ubuntu)
sudo apt install steamcmd
```

**Download the game files:**
```bash
steamcmd \
  +@sSteamCmdForcePlatformType windows \
  +login YOUR_STEAM_USERNAME \
  +force_install_dir ./generals_files \
  +app_update 2732940 validate \
  +quit

steamcmd \
  +@sSteamCmdForcePlatformType windows \
  +login YOUR_STEAM_USERNAME \
  +force_install_dir ./zh_files \
  +app_update 2732960 validate \
  +quit
```

Steam will prompt for your password and Steam Guard code.

App IDs: `2732940` = Generals, `2732960` = Zero Hour.

**Copy to runtime directory:**
```bash
cp ./generals_files/*.big ~/TheSuperHackers/GeneralsZH/
cp ./zh_files/*.big ~/TheSuperHackers/GeneralsZH/
```

## Option 3 — CrossOver Trial (No Windows PC, No Command Line)

CrossOver lets you run Windows software on macOS. CodeWeavers offers a free 14-day trial.

1. Download CrossOver from https://www.codeweavers.com/crossover
2. Install Steam inside CrossOver
3. Log in and download Generals Zero Hour
4. Copy the `.big` files from the CrossOver bottle to your runtime directory

The default bottle path is:
```
~/Library/Application Support/CrossOver/Bottles/<bottle>/drive_c/Program Files (x86)/Steam/steamapps/common/
```

## Required Files

At minimum you need these `.big` files from both Generals and Zero Hour:

**Zero Hour:**
- `INIZH.big` — game rules and configuration
- `W3DZH.big` — 3D models
- `TexturesZH.big` — textures
- `MapsZH.big` — maps
- `WindowZH.big` — UI definitions
- `EnglishZH.big` — localized text (use your language variant)

**Generals (base game):**
- `INI.big`, `W3D.big`, `Textures.big`, `Maps.big`, `Window.big`, `English.big`

**Optional (audio/music):**
- `MusicZH.big`, `AudioZH.big`, `AudioEnglishZH.big`, `SpeechEnglishZH.big`
- `Music.big`, `Audio.big`, `AudioEnglish.big`, `SpeechEnglish.big`, `Shaders.big`

Without the audio files the game runs fine — set `GGC_NO_AUDIO=1` to suppress audio warnings.

## Open-Source Game Data

Some game data (INI scripts, UI definitions, art overrides) is maintained in the open-source patch repository. Fetch it with:

```bash
scripts/build/macos/fetch-game-data.sh
```

This pulls from [TheSuperHackers/GeneralsGamePatch](https://github.com/TheSuperHackers/GeneralsGamePatch) and stages `Data/`, `Window/`, and `Art/` into your runtime directory. These files override the corresponding entries in the `.big` archives.

## Next Steps

Once game files are in place, build and deploy the engine:
- [macOS Build Guide](../../README.md#macos-apple-silicon--intel)
- [Windows Build Guide](../../README.md#windows-visual-studio-2022)
- [Linux Build Guide](../../README.md#linux-via-docker)
