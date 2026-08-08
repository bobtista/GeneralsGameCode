# Installing and Playing

These are experimental rolling builds of *Command & Conquer: Generals — Zero Hour* for Windows, macOS, and Linux.

The builds are automatically updated from the development branch. They are not stable upstream releases, so keep
`BUILD_INFO.txt` when reporting a problem — it identifies the exact commit you downloaded.

You must own *Command & Conquer: Generals* and *Zero Hour*. The downloads contain the game engine and required runtime
libraries, but no EA game assets.

## Supported Platforms

| Platform | Requirements | Download |
| --- | --- | --- |
| Windows | 64-bit Windows | `GeneralsZH-win64.zip` |
| macOS | Apple Silicon and macOS 15 or newer | `GeneralsZH-macos-arm64.zip` |
| Linux | x86_64, GLIBC 2.38 or newer, Vulkan | `GeneralsZH-linux-x64.zip` |

There are no Intel Mac or 32-bit builds.

Download the files from the
[latest bgfx rolling release](https://github.com/bobtista/GeneralsGameCode/releases/tag/latest-bgfx).

Windows users should normally download the full `GeneralsZH-win64.zip`. The `exe-only` archives are intended only for
updating an existing matching installation whose SDL3, OpenAL, and FFmpeg libraries are already present.

The Windows debug archives contain debugging symbols and are intended for diagnosing bugs.

## Required Game Data

You need data from both games:

- *Command & Conquer: Generals*
- *Command & Conquer: Generals — Zero Hour*

Copy all `.big` archives from both installations.

You also need these loose data directories when present:

- `Data/Scripts/`
- `Data/Cursors/`
- `Data/Movies/`
- `Data/<language>/Movies/`

These contain skirmish and multiplayer scripts, mouse cursors, and videos that are not stored in the `.big` archives.

When merging loose files from both games, let the Zero Hour versions win if the same file exists in both places.

Do not overwrite the build's supplied `Data/INI/Bgfx.ini`.

For ways to obtain the retail files through an existing installation, SteamCMD, or CrossOver, see
[Getting the Game Files](https://github.com/bobtista/GeneralsGameCode/blob/bobtista/topic/trunk/docs/BUILD/GETTING_THE_GAME_FILES.md).
After obtaining the game, return here and copy the loose directories listed above as well as the `.big` archives.

## Windows Installation

1. Install the
   [Microsoft Visual C++ 2022 x64 Redistributable](https://aka.ms/vs/17/release/vc_redist.x64.exe).
2. Extract `GeneralsZH-win64.zip`. It creates a `GeneralsZH-win64` folder.
3. Copy the retail `.big` archives and loose data into that folder.
4. Launch the game.

From PowerShell:

```powershell
cd "C:\path\to\GeneralsZH-win64"
.\generalszh.exe -win
```

The `-win` option starts the game windowed. Omit it for fullscreen.

You can alternatively copy the contents of `GeneralsZH-win64` into an existing complete Zero Hour installation,
provided the base Generals installation is also available to the engine.

## macOS Installation

The published build requires an Apple Silicon Mac running macOS 15 or newer.

Extract the archive:

```bash
mkdir -p ~/Games
unzip ~/Downloads/GeneralsZH-macos-arm64.zip -d ~/Games
cd ~/Games/GeneralsZH-macos-arm64
```

Copy the retail `.big` archives and loose data into this folder, then launch:

```bash
./run.sh -win
```

The executable is ad-hoc signed but not Apple-notarized. If macOS blocks it, first confirm that you downloaded it from
this repository. Then use either:

- **System Settings → Privacy & Security → Open Anyway**, or
- the following command, applied only to the extracted game folder:

```bash
xattr -dr com.apple.quarantine ~/Games/GeneralsZH-macos-arm64
```

Run `./run.sh -win` again afterward.

No Homebrew packages are required to run the downloaded build.

## Linux Installation

The prebuilt Linux executable requires a glibc-based x86_64 distribution with GLIBC 2.38 or newer.

Check your installed version:

```bash
ldd --version
```

On Debian or Ubuntu with an AMD or Intel GPU, install the common runtime dependencies with:

```bash
sudo apt update
sudo apt install libvulkan1 mesa-vulkan-drivers libfreetype6 libfontconfig1
```

For NVIDIA, install the Vulkan support supplied with the proprietary NVIDIA driver instead of relying on Mesa.

Extract the archive:

```bash
mkdir -p ~/Games
unzip ~/Downloads/GeneralsZH-linux-x64.zip -d ~/Games
cd ~/Games/GeneralsZH-linux-x64
```

If you already have the game installed through Wine, Proton, Steam, or Lutris, import its data automatically:

```bash
./import-from-wine.sh
```

Useful options include:

```bash
# Show what would be copied without changing anything
./import-from-wine.sh --dry-run

# Also import options, saves, maps, and replays
./import-from-wine.sh --with-saves

# Search an additional game folder, Steam library, or Wine prefix
./import-from-wine.sh --prefix "/path/to/game-or-prefix"

# Overwrite files already imported
./import-from-wine.sh --force
```

Launch the game:

```bash
./run.sh -win
```

## Keeping the Archives Elsewhere

On macOS and Linux, the engine can mount the retail `.big` archives from separate directories:

```bash
CNC_ZH_INSTALLPATH="/path/to/Zero Hour" \
CNC_GENERALS_INSTALLPATH="/path/to/Generals" \
./run.sh -win
```

These variables redirect `.big` archive loading only. They do not redirect loose files. Keep `Data/Scripts`,
`Data/Cursors`, and `Data/Movies` in the engine's runtime folder.

## Mods

Some data-only mods can run as overlays. Compatibility varies.

Mods that patch the retail executable, require their own Windows launcher, or depend on the original 32-bit engine will
not work without adaptation.

Place a mod's `.big` files in a named directory under the per-user data folder:

| Platform | Example for a mod named `Shockwave` |
| --- | --- |
| Windows | `Documents\Command and Conquer Generals Zero Hour Data\Shockwave\` |
| macOS | `~/Library/Application Support/Command and Conquer Generals Zero Hour Data/Shockwave/` |
| Linux | `$XDG_DATA_HOME/Command and Conquer Generals Zero Hour Data/Shockwave/`, or `~/.local/share/Command and Conquer Generals Zero Hour Data/Shockwave/` |

Launch on macOS or Linux with:

```bash
./run.sh -win -mod Shockwave
```

Launch on Windows with:

```powershell
.\generalszh.exe -win -mod Shockwave
```

All multiplayer participants must use compatible game and mod data.

## Renderer Configuration

Each build includes `Data/INI/Bgfx.ini`. The supplied file enables the modern sun shadow map and disables classic
stencil shadows. Leave this file in place when copying the retail data.

## Troubleshooting

### The Game Closes Immediately or Assets Are Missing

Confirm that `.big` archives from both Generals and Zero Hour are available.

Also confirm that you copied the loose Scripts, Cursors, and Movies directories.

### Skirmish AI Does Nothing

Confirm that this file exists:

```text
Data/Scripts/SkirmishScripts.scb
```

The skirmish scripts are not stored in the `.big` archives.

### macOS Says the Developer Cannot Be Verified

Use **Open Anyway** or clear quarantine as described in the macOS installation section.

### Linux Reports a GLIBC Version Error

The prebuilt executable requires GLIBC 2.38 or newer. Upgrade to a newer distribution or build the project from source.

### Linux Reports a Vulkan or Device-Creation Error

Install `vulkan-tools`, then check the driver with:

```bash
vulkaninfo --summary
```

Install the appropriate Vulkan driver for your GPU.

### Windows Reports a Missing DLL

Install the Microsoft Visual C++ x64 redistributable and use the full `GeneralsZH-win64.zip`, not an `exe-only`
archive.

### A Mod Appears Not to Load

Check the terminal output for:

```text
[ggc] mod not found
```

Confirm that the name passed to `-mod` matches the directory name exactly.

## Reporting a Problem

Open an issue in the [fork's issue tracker](https://github.com/bobtista/GeneralsGameCode/issues) and include:

- `BUILD_INFO.txt`
- `DebugLogFile.txt` from the game folder, beside the executable
- Operating system and version
- CPU architecture
- GPU and driver version
- The exact launch command
- Whether a mod was enabled
- What you expected to happen
- What actually happened
- A save or replay when relevant

These builds write `DebugLogFile.txt` next to the executable, and move the previous run's log
to `DebugLogFilePrev.txt`. If the game crashed and you have since relaunched it, the crash is in
`DebugLogFilePrev.txt`, so attach that one.

On Windows, also attach any `Crash*.dmp` files from
`Documents\Command and Conquer Generals Zero Hour Data\`.

On macOS or Linux, capture terminal output with:

```bash
./run.sh -win 2>&1 | tee ggc.log
```

Attach `ggc.log` to the report.

If you reproduce a Windows problem using the debug archive, attach any generated `DebugLogFileD.txt`, crash dumps, and
stack-dump files.

For installation questions rather than bugs, use
[GitHub Discussions](https://github.com/bobtista/GeneralsGameCode/discussions).

## Building from Source

See
[Building the Game Yourself](https://github.com/bobtista/GeneralsGameCode/blob/bobtista/topic/trunk/README.md#building-the-game-yourself).

## Legal

EA has not endorsed or supported these builds. All trademarks belong to their respective owners.

You must own legitimate copies of *Command & Conquer: Generals* and *Zero Hour*. No retail game assets are distributed
with these builds.

GeneralsGameCode is distributed under the GNU General Public License version 3. See `LICENSE.md` for the complete
license.
