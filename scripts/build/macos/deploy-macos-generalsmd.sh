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

collect_deployed_dylibs()
{
    local image
    local dep
    local path
    local -a queue=("$1")
    local seen=$'\n'

    while ((${#queue[@]} > 0)); do
        image="${queue[0]}"
        queue=("${queue[@]:1}")
        while IFS= read -r dep; do
            path="${runtime_dir}/${dep#@executable_path/}"
            if [[ -f "${path}" && "${seen}" != *$'\n'"${path}"$'\n'* ]]; then
                seen+="${path}"$'\n'
                printf '%s\n' "${path}"
                queue+=("${path}")
            fi
        done < <(otool -L "${image}" 2>/dev/null | awk '/@executable_path\/.*\.dylib/ { print $1 }')
    done
}

list_rpaths()
{
    local image="$1"
    otool -l "${image}" 2>/dev/null \
        | awk '
            /cmd LC_RPATH/ { in_rpath = 1; next }
            in_rpath && /path / { print $2; in_rpath = 0 }
        '
}

dedupe_rpaths()
{
    local image="$1"
    local rpath
    local -a duplicates=()
    local seen=$'\n'
    local duplicate_seen=$'\n'

    while IFS= read -r rpath; do
        if [[ "${seen}" == *$'\n'"${rpath}"$'\n'* ]]; then
            if [[ "${duplicate_seen}" != *$'\n'"${rpath}"$'\n'* ]]; then
                duplicates+=("${rpath}")
                duplicate_seen+="${rpath}"$'\n'
            fi
        else
            seen+="${rpath}"$'\n'
        fi
    done < <(list_rpaths "${image}")

    if ((${#duplicates[@]} == 0)); then
        return
    fi

    for rpath in "${duplicates[@]}"; do
        while list_rpaths "${image}" | grep -Fqx "${rpath}"; do
            install_name_tool -delete_rpath "${rpath}" "${image}" 2>/dev/null || break
        done
        install_name_tool -add_rpath "${rpath}" "${image}"
    done
}

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
    dylib_search_paths=(
        "${binary_dir}/_deps/sdl3-build/${config}"
        "${binary_dir}/_deps/sdl3-build/Release"
        "${binary_dir}/_deps/sdl3-build/RelWithDebInfo"
        "${binary_dir}/_deps/openal_soft-build/${config}"
        "${binary_dir}/_deps/openal_soft-build/Release"
        "${binary_dir}/_deps/openal_soft-build/RelWithDebInfo"
        "/opt/homebrew/lib"
        "/opt/homebrew/opt/ffmpeg/lib"
        "/usr/local/lib"
    )
    dylib_search_args=()
    for search_path in "${dylib_search_paths[@]}"; do
        if [[ -d "${search_path}" ]]; then
            dylib_search_args+=( -s "${search_path}" )
        fi
    done
    dylibbundler \
        -of \
        -cd \
        -b \
        -x "${runtime_dir}/${binary_name}" \
        -d "${runtime_dir}/" \
        -p "@executable_path/" \
        "${dylib_search_args[@]}" \
        -i /usr/lib \
        -i /System/Library
else
    if [[ "${GGC_MACOS_SKIP_DYLIB_BUNDLING:-0}" == "1" ]]; then
        echo "WARNING: dylibbundler not found; skipping because GGC_MACOS_SKIP_DYLIB_BUNDLING=1." >&2
    else
        echo "ERROR: dylibbundler not found; dylib dependencies were not staged." >&2
        echo "       Install with 'brew install dylibbundler' or set GGC_MACOS_SKIP_DYLIB_BUNDLING=1." >&2
        exit 1
    fi
fi

if command -v install_name_tool >/dev/null 2>&1; then
    echo "  Deduplicating deployed rpaths..."
    dedupe_rpaths "${runtime_dir}/${binary_name}"
    while IFS= read -r dylib; do
        dedupe_rpaths "${dylib}"
    done < <(collect_deployed_dylibs "${runtime_dir}/${binary_name}" | sort -u)
fi

if command -v codesign >/dev/null 2>&1; then
    echo "  Ad-hoc signing deployed executable and dylibs..."
    while IFS= read -r dylib; do
        codesign --force --sign - "${dylib}"
    done < <(collect_deployed_dylibs "${runtime_dir}/${binary_name}" | sort -u)
    codesign --force --sign - "${runtime_dir}/${binary_name}"
    codesign --verify --strict "${runtime_dir}/${binary_name}"
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
