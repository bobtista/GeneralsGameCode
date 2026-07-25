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

# Bundle the binary's shared-library closure EXCEPT the libraries that must come
# from the host: glibc / libstdc++, and the display / GPU / audio stack that
# talks to the user's drivers and display server. This captures our FetchContent
# SDL3 / OpenAL and the bundled minimal FFmpeg (avcodec/format/util/swscale/
# swresample), which has no external codec deps, so the closure stays small.
# freetype / fontconfig are ABI-stable and present on every desktop distro, so
# they are left to the host to avoid shipping a stale font-config stack.
host_lib_re='^(ld-linux.*|libc\.so.*|libm\.so.*|libdl\.so.*|libpthread\.so.*|librt\.so.*|libutil\.so.*|libresolv\.so.*|libanl\.so.*|libnss_.*|libnsl\.so.*|libstdc\+\+\.so.*|libgcc_s\.so.*|libfreetype\.so.*|libfontconfig\.so.*|libexpat\.so.*|libX.*|libxcb.*|libxshmfence.*|libwayland.*|libvulkan\.so.*|libGL.*|libEGL.*|libGLX.*|libGLdispatch.*|libOpenGL.*|libglapi.*|libgbm.*|libdrm.*|libva.*|libvdpau.*|libasound\.so.*|libpulse.*|libpulsecommon.*|libjack.*|libpipewire.*|libglvnd.*)$'

# Seed LD_LIBRARY_PATH from the binary's own RUNPATH (the FFmpeg prefix and the
# FetchContent build dirs), so ldd resolves the FULL transitive closure - notably
# libswresample, which libavcodec needs but the game never links directly. A
# DT_RUNPATH only resolves an object's own direct deps, so without this the
# transitive libs show up as "not found" and would be missed.
runpaths="$(readelf -d "${bin}" 2>/dev/null | sed -n 's/.*R\(UN\)\?PATH.*\[\(.*\)\]/\2/p')"
if [ -n "${runpaths}" ]; then
	export LD_LIBRARY_PATH="${runpaths}:${LD_LIBRARY_PATH:-}"
fi

ldd "${bin}" | awk '/=>/ && $3 ~ /\// { print $3 }' | while read -r lib; do
	name="$(basename "${lib}")"
	if [[ "${name}" =~ ${host_lib_re} ]]; then
		continue
	fi
	cp -vL "${lib}" "${stage_dir}/libs/"
done

# Fail loudly if any needed library could not be resolved (would ship a broken zip).
if ldd "${bin}" | grep -q "not found"; then
	echo "ERROR: unresolved shared libraries:" >&2
	ldd "${bin}" | grep "not found" >&2
	exit 1
fi

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
