#!/usr/bin/env bash
set -euo pipefail

# TheSuperHackers @build bobtista 29/04/2026 Pull the source-controlled
# Zero Hour game data files (INI rules, .wnd UI defs, language strings) from
# TheSuperHackers/GeneralsGamePatch into the macOS runtime directory.
#
# Retail .big files (Maps, Models, Textures, Audio, Music, Speech, Shaders)
# are NOT in that repo. Copy both the base Generals and Zero Hour archives from
# your retail install separately.
#
# Default runtime dir: ~/TheSuperHackers/GeneralsZH/
# Override with GGC_MACOS_RUNTIME_DIR=/path/to/install/dir

repo_url="https://github.com/TheSuperHackers/GeneralsGamePatch.git"
cache_dir="${GGC_GAMEDATA_CACHE:-${HOME}/.cache/TheSuperHackers/GeneralsGamePatch}"
sparse_path="Patch104pZH/GameFilesOriginalZH"
runtime_dir="${GGC_MACOS_RUNTIME_DIR:-${HOME}/TheSuperHackers/GeneralsZH}"

mkdir -p "${runtime_dir}"

if [[ ! -d "${cache_dir}/.git" ]]; then
    echo "Cloning ${repo_url} (sparse: ${sparse_path}) into ${cache_dir}..."
    mkdir -p "$(dirname "${cache_dir}")"
    git clone --depth=1 --filter=blob:none --sparse "${repo_url}" "${cache_dir}"
    git -C "${cache_dir}" sparse-checkout set "${sparse_path}"
else
    echo "Updating cache at ${cache_dir}..."
    git -C "${cache_dir}" sparse-checkout set "${sparse_path}"
    git -C "${cache_dir}" pull --depth=1 --ff-only
fi

src_dir="${cache_dir}/${sparse_path}"
if [[ ! -d "${src_dir}" ]]; then
    echo "ERROR: ${src_dir} missing after clone." >&2
    exit 1
fi

echo "Staging Data/, Window/, Art/ into ${runtime_dir}/"
for sub in Data Window Art; do
    if [[ -d "${src_dir}/${sub}" ]]; then
        rsync -a "${src_dir}/${sub}/" "${runtime_dir}/${sub}/"
    fi
done

# Stage the annotated Bgfx.ini template only when the file is absent, so local
# render setting overrides survive re-runs.
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
if [[ ! -f "${runtime_dir}/Data/INI/Bgfx.ini" ]]; then
    echo "Staging Data/INI/Bgfx.ini template"
    mkdir -p "${runtime_dir}/Data/INI"
    cp "${script_dir}/Bgfx.ini" "${runtime_dir}/Data/INI/Bgfx.ini"
fi

echo ""
echo "Source-controlled game data staged."
echo ""
echo "You still need retail .big files from both Generals and Zero Hour:"
echo "  Zero Hour: INIZH.big  WindowZH.big  EnglishZH.big  MapsZH.big"
echo "             W3DZH.big  TexturesZH.big  MusicZH.big  SpeechEnglishZH.big"
echo "             AudioZH.big  AudioEnglishZH.big"
echo "  Generals:  INI.big  Window.big  English.big  Maps.big  W3D.big"
echo "             Textures.big  Music.big  SpeechEnglish.big  Audio.big"
echo "             AudioEnglish.big  Shaders.big"
echo "  Cursors:   extracted Data/Cursors/*.ani"
echo "  Scripts:   extracted Data/Scripts/SkirmishScripts.scb"
echo "             extracted Data/Scripts/MultiplayerScripts.scb"
echo ""
echo "Drop them next to the binary at:"
echo "  ${runtime_dir}/"
echo "Drop cursor .ani files at:"
echo "  ${runtime_dir}/Data/Cursors/"
echo "Drop script .scb files at:"
echo "  ${runtime_dir}/Data/Scripts/"
