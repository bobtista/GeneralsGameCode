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

echo "  Writing run.sh wrapper..."
cat > "${runtime_dir}/run.sh" <<'WRAPPER'
#!/usr/bin/env bash
# TheSuperHackers @build bobtista 30/04/2026 GeneralsMD macOS launch wrapper.
#
# Apple's AGX shader compiler intermittently faults inside
# AGCDeserializedReply when bgfx asks it to compile per-framebuffer
# helper shaders for our 14-view setup. The fault is on a background
# dispatch queue, lands in the first 1-2s of run-time, and can't be
# caught from inside the process. The engine itself is fine once it
# wins the race - 174s+ continuous runs captured. Until Apple fixes
# the driver bug, the practical workaround is to retry on a fast-fail
# exit code. Set GGC_MACOS_NO_RETRY=1 to disable.
set -uo pipefail
script_dir="$(cd "$(dirname "$0")" && pwd)"
cd "${script_dir}"
export DYLD_LIBRARY_PATH="${script_dir}:${DYLD_LIBRARY_PATH:-}"

if [[ "${GGC_MACOS_NO_RETRY:-0}" == "1" ]]; then
    exec "${script_dir}/generalszh" "$@"
fi

readonly max_attempts="${GGC_MACOS_LAUNCH_ATTEMPTS:-200}"
readonly fast_fail_seconds="${GGC_MACOS_FAST_FAIL_SECONDS:-15}"
readonly retry_sleep="${GGC_MACOS_RETRY_SLEEP:-0.1}"

echo "[run.sh] Apple Silicon AGX startup race in play; the wrapper will" >&2
echo "[run.sh] silently relaunch on fast SIGSEGV/SIGABRT until one wins." >&2
echo "[run.sh] First successful launch is usually within 1-30 attempts." >&2

# TheSuperHackers @build bobtista 30/04/2026 Run generalszh in the
# foreground (no background+wait). Ctrl+C in the controlling terminal
# is sent to the whole process group, so the child is taken down with
# the wrapper and no zombies are left. Backgrounding the child broke
# WindowServer foreground association on macOS - the dock icon appeared
# but no window ever came up. The last attempt is exec'd to flatten
# the process tree once we know the AGX race has been won.
for ((attempt=1; attempt<=max_attempts; ++attempt)); do
    start_epoch=$(date +%s)
    if [[ ${attempt} -eq ${max_attempts} ]]; then
        exec "${script_dir}/generalszh" "$@"
    fi
    "${script_dir}/generalszh" "$@"
    rc=$?
    end_epoch=$(date +%s)
    elapsed=$((end_epoch - start_epoch))

    # Clean exits and slow failures get propagated as-is.
    if [[ ${rc} -eq 0 ]] || [[ ${elapsed} -ge ${fast_fail_seconds} ]]; then
        exit ${rc}
    fi

    # Only retry on the fast-fail signal exits we attribute to the AGX
    # race. SIGABRT(134), SIGBUS(138), and SIGSEGV(139). NOT SIGKILL(137)
    # - that's almost always the user pressing Ctrl-quit / killing the
    # dock icon, and a retry loop on it would be unbreakable.
    if [[ ${rc} -ne 134 && ${rc} -ne 138 && ${rc} -ne 139 ]]; then
        echo "[run.sh] generalszh exited with ${rc} after ${elapsed}s; not retrying." >&2
        exit ${rc}
    fi

    if [[ ${attempt} -lt ${max_attempts} ]]; then
        echo "[run.sh] AGX startup race lost (exit ${rc} in ${elapsed}s); retrying ${attempt}/${max_attempts}..." >&2
        sleep "${retry_sleep}"
    fi
done

# The for-loop's last iteration always exec's, so this is unreachable;
# kept only as a defensive trap in case the loop structure is ever
# refactored to drop the exec-on-last-attempt special case.
echo "[run.sh] Gave up after ${max_attempts} attempts. Set GGC_MACOS_NO_RETRY=1 or raise GGC_MACOS_LAUNCH_ATTEMPTS." >&2
exit "${rc:-1}"
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
echo "Place your retail Zero Hour .big files in ${runtime_dir}/ before launching."
