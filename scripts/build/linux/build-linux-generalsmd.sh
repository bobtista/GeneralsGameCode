#!/usr/bin/env bash
set -euo pipefail

# TheSuperHackers @build bobtista 24/07/2026 Native Linux build of the GeneralsMD
# SDL3/bgfx client inside a Debian container. Builds the z_generals (Zero Hour)
# target for the linux-generalsmd-sdl3-bgfx preset. The build directory and
# FetchContent deps live in a named Docker volume so re-runs are incremental.
#
# Usage:
#   scripts/build/linux/build-linux-generalsmd.sh [cmake-target]
#
# The produced ELF lands in the volume at build/${preset}/GeneralsMD/generalszh;
# copy it out with:  scripts/build/linux/copy-out.sh   (or docker cp from a run)

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
image="ggc-linux-build"
preset="linux-generalsmd-sdl3-bgfx"
target="${1:-z_generals}"
build_volume="ggc-linux-build"

docker build -t "${image}" "${repo_root}/scripts/build/linux"

docker run --rm \
    -v "${repo_root}:/src" \
    -v "${build_volume}:/src/build" \
    -w /src \
    "${image}" \
    bash -euo pipefail -c "
        bash scripts/build/linux/build-ffmpeg-minimal.sh /src/build/ffmpeg-min
        export PKG_CONFIG_PATH=/src/build/ffmpeg-min/lib/pkgconfig
        cmake --preset ${preset}
        cmake --build build/${preset} --target ${target} -j\$(nproc)
    "
