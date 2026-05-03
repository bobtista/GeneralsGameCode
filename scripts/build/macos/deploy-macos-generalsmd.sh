#!/usr/bin/env bash
set -euo pipefail

# TheSuperHackers @build bobtista 28/04/2026 Flat-directory deploy for the
# GeneralsMD macOS build. Copies the built binary into a runtime directory next
# to the user's .big assets, bundles its dylib dependencies via dylibbundler,
# and writes a run.sh wrapper.
#
# Default runtime dir: ~/TheSuperHackers/GeneralsZH/
# Override with GGC_MACOS_RUNTIME_DIR=/path/to/install/dir

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
preset="${1:-${GGC_MACOS_PRESET:-macos-generalsmd-sdl3-bgfx}}"
config="${2:-${GGC_MACOS_CONFIG:-Release}}"
binary_dir="${repo_root}/build/${preset}"
runtime_dir="${GGC_MACOS_RUNTIME_DIR:-${HOME}/TheSuperHackers/GeneralsZH}"

readonly binary_name="generalszh"
candidate_binaries=(
    "${binary_dir}/GeneralsMD/${config}/${binary_name}"
    "${binary_dir}/GeneralsMD/${binary_name}"
    "${binary_dir}/GeneralsMD/Code/Main/${config}/${binary_name}"
    "${binary_dir}/GeneralsMD/Code/Main/${binary_name}"
    "${binary_dir}/${config}/${binary_name}"
    "${binary_dir}/${binary_name}"
)
binary_path=""
for candidate in "${candidate_binaries[@]}"; do
    if [[ -x "${candidate}" ]]; then
        binary_path="${candidate}"
        break
    fi
done
if [[ -z "${binary_path}" ]]; then
    echo "ERROR: GeneralsMD binary not found. Checked:" >&2
    printf '  %s\n' "${candidate_binaries[@]}" >&2
    exit 1
fi

mkdir -p "${runtime_dir}"

echo "Deploying ${binary_path}"
echo "         to ${runtime_dir}"

cp -f "${binary_path}" "${runtime_dir}/${binary_name}"
chmod +x "${runtime_dir}/${binary_name}"

if command -v dylibbundler >/dev/null 2>&1; then
    echo "  Bundling dylib dependencies via dylibbundler..."
    dylibbundler \
        -od \
        -b \
        -x "${runtime_dir}/${binary_name}" \
        -d "${runtime_dir}/" \
        -p "@executable_path/" \
        -i /usr/lib \
        -i /System/Library
else
    echo "WARNING: dylibbundler not found; dylib dependencies were not staged." >&2
    echo "         Install with 'brew install dylibbundler' to enable bundling." >&2
fi

if command -v codesign >/dev/null 2>&1; then
    echo "  Ad-hoc signing deployed executable..."
    codesign --force --sign - "${runtime_dir}/${binary_name}"
fi

echo "  Writing run.sh wrapper..."
cat > "${runtime_dir}/run.sh" <<'WRAPPER'
#!/usr/bin/env bash
# TheSuperHackers @build bobtista 30/04/2026 GeneralsMD macOS launch wrapper.
set -euo pipefail
script_dir="$(cd "$(dirname "$0")" && pwd)"
cd "${script_dir}"
export DYLD_LIBRARY_PATH="${script_dir}:${DYLD_LIBRARY_PATH:-}"
exec "${script_dir}/generalszh" "$@"
WRAPPER
chmod +x "${runtime_dir}/run.sh"

echo ""
echo "Deploy complete"
echo "   Executable: ${runtime_dir}/${binary_name}"
echo "   Wrapper:    ${runtime_dir}/run.sh"
echo ""
echo "Run with:"
echo "  ${runtime_dir}/run.sh"
echo ""
echo "Place your retail Generals + Zero Hour .big files in ${runtime_dir}/ before launching."
