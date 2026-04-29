#!/usr/bin/env bash
set -euo pipefail

# TheSuperHackers @build bobtista 29/04/2026 Pull the source-controlled
# Zero Hour game data files (INI rules, .wnd UI defs, language strings) from
# TheSuperHackers/GeneralsGamePatch into the macOS runtime directory.
#
# Retail .big files (Maps, Models, Textures, Audio, Music, Speech) are NOT
# in that repo — copy them from your retail Zero Hour install separately.
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

echo ""
echo "Source-controlled game data staged."
echo ""
echo "You still need the retail .big files from your Zero Hour install:"
echo "  INIZH.big       WindowZH.big    EnglishZH.big   (or German/French/etc.)"
echo "  MapsZH.big      W3DZH.big       TexturesZH.big"
echo "  MusicZH.big     SpeechZH.big    AudioZH.big"
echo ""
echo "Drop them next to the binary at:"
echo "  ${runtime_dir}/"
