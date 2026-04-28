#!/usr/bin/env bash
set -euo pipefail

# TheSuperHackers @build bobtista 28/04/2026 GeneralsMD-only macOS build helper.
# Configures and builds the bgfx/SDL3 preset, then deploys the binary plus its
# dylib dependencies to the runtime directory chosen by deploy-macos-generalsmd.sh.

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
preset="${GGC_MACOS_PRESET:-macos-generalsmd-sdl3-bgfx}"
config="${GGC_MACOS_CONFIG:-Release}"
target="${GGC_MACOS_TARGET:-z_generals}"

cmake --preset "${preset}" "${repo_root}"
cmake --build "${repo_root}/build/${preset}" --config "${config}" --target "${target}"

"${repo_root}/scripts/build/macos/deploy-macos-generalsmd.sh" "${preset}" "${config}"
