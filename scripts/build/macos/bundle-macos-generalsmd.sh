#!/usr/bin/env bash
set -euo pipefail

# TheSuperHackers @build bobtista 28/04/2026 Create an unsigned local
# GeneralsMD .app bundle from the macOS SDL3/bgfx build output.

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
preset="${1:-${GGC_MACOS_PRESET:-macos-generalsmd-sdl3-bgfx}}"
config="${2:-${GGC_MACOS_CONFIG:-Release}}"
binary_dir="${repo_root}/build/${preset}"
bundle_dir="${binary_dir}/bundle/GeneralsMD.app"
contents_dir="${bundle_dir}/Contents"
macos_dir="${contents_dir}/MacOS"
resources_dir="${contents_dir}/Resources"
frameworks_dir="${contents_dir}/Frameworks"

binary_name="generalszh"
binary_path="${binary_dir}/GeneralsMD/Code/Main/${config}/${binary_name}"
if [[ ! -x "${binary_path}" ]]; then
    binary_path="${binary_dir}/GeneralsMD/Code/Main/${binary_name}"
fi
if [[ ! -x "${binary_path}" ]]; then
    echo "Missing GeneralsMD binary: ${binary_path}" >&2
    exit 1
fi

rm -rf "${bundle_dir}"
mkdir -p "${macos_dir}" "${resources_dir}" "${frameworks_dir}"

cp "${binary_path}" "${macos_dir}/GeneralsMD"
cat > "${contents_dir}/Info.plist" <<'PLIST'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleExecutable</key>
    <string>GeneralsMD</string>
    <key>CFBundleIdentifier</key>
    <string>com.thesuperhackers.generalsmd</string>
    <key>CFBundleName</key>
    <string>GeneralsMD</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleShortVersionString</key>
    <string>0.1</string>
    <key>NSHighResolutionCapable</key>
    <true/>
</dict>
</plist>
PLIST

if command -v dylibbundler >/dev/null 2>&1; then
    dylibbundler \
        -od \
        -b \
        -x "${macos_dir}/GeneralsMD" \
        -d "${frameworks_dir}" \
        -p "@executable_path/../Frameworks"
else
    echo "dylibbundler not found; dependencies were not copied into the bundle." >&2
fi

echo "Created ${bundle_dir}"
