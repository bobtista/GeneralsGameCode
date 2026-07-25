#!/usr/bin/env bash
set -euo pipefail

# TheSuperHackers @build bobtista 24/07/2026 Stage the native Linux GeneralsMD
# build for distribution: the ELF, its bundled non-system shared libraries, and a
# run.sh wrapper that puts them on LD_LIBRARY_PATH. System libraries (glibc, Vulkan
# loader, X11/Wayland, Mesa, FFmpeg, ALSA/Pulse) are expected on the user's machine
# and are NOT bundled.
#
# Usage:
#   scripts/build/linux/deploy-linux-generalsmd.sh [preset] [config]
# Override the staging dir with GGC_LINUX_RUNTIME_DIR.

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
preset="${1:-linux-generalsmd-sdl3-bgfx}"
config="${2:-Release}"
stage_dir="${GGC_LINUX_RUNTIME_DIR:-${repo_root}/stage-linux/GeneralsZH-linux}"
bin="${repo_root}/build/${preset}/GeneralsMD/${config}/generalszh"

if [ ! -x "${bin}" ]; then
	echo "Error: binary not found or not executable: ${bin}" >&2
	exit 1
fi

rm -rf "${stage_dir}"
mkdir -p "${stage_dir}/libs"
cp -v "${bin}" "${stage_dir}/generalszh"

# Bundle only shared libs resolved out of the build tree (FetchContent-built
# SDL3/OpenAL/zlib etc.). Everything under a system path is left to the host.
ldd "${bin}" | awk '/=>/ && $3 ~ /\// { print $3 }' | while read -r lib; do
	case "${lib}" in
		*/build/*|*_deps*)
			cp -vL "${lib}" "${stage_dir}/libs/"
			;;
	esac
done

cat > "${stage_dir}/run.sh" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
export LD_LIBRARY_PATH="${here}/libs:${LD_LIBRARY_PATH:-}"
exec "${here}/generalszh" "$@"
EOF
chmod +x "${stage_dir}/run.sh"

# Ship the WINE-migration helper so players can pull their existing .big assets
# over without reinstalling.
cp -v "${repo_root}/scripts/build/linux/import-from-wine.sh" "${stage_dir}/import-from-wine.sh"
chmod +x "${stage_dir}/import-from-wine.sh"

# Ship the default render settings (sun shadow map on) unless one already exists.
mkdir -p "${stage_dir}/Data/INI"
cp -n "${repo_root}/scripts/build/dist/Bgfx.ini" "${stage_dir}/Data/INI/Bgfx.ini"

echo "Staged Linux build to: ${stage_dir}"
ls -la "${stage_dir}" "${stage_dir}/libs"
