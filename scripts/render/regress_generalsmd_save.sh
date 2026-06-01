#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'USAGE'
Usage:
  regress_generalsmd_save.sh --save 00000053.sav --reference-dir .context/references/save53 --actual-dir .context/captures/save53-latest [options]

Options:
  --runtime-dir DIR             Deployed GeneralsZH directory. Default: $GGC_MACOS_RUNTIME_DIR or ~/TheSuperHackers/GeneralsZH
  --save NAME                   Save basename to pass to -loadsave.
  --reference-dir DIR           Directory containing reference BMP captures.
  --actual-dir DIR              Directory for a fresh capture.
  --diff-dir DIR                Directory for optional BMP diff images.
  --pattern GLOB                Capture filename pattern to compare. Default: *.bmp.
  --threshold N                 Per-channel pixel threshold. Default: 8.
  --max-differing-percent N     Allowed differing pixel percentage. Default: 0.0.
  --frame N                     First bgfx frame to request. Default: 120.
  --interval N                  Screenshot interval in bgfx frames. Default: 30.
  --duration SECONDS            Wall-clock run duration before graceful stop. Default: 20.
  --fullscreen                  Run without -win.
  --extra-arg ARG               Extra argument passed through to generalszh. Repeatable.
  -h, --help                    Show this help.
USAGE
}

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

runtime_dir="${GGC_MACOS_RUNTIME_DIR:-${HOME}/TheSuperHackers/GeneralsZH}"
save_name=""
reference_dir=""
actual_dir=""
diff_dir=""
pattern="*.bmp"
threshold="8"
max_differing_percent="0.0"
capture_frame="120"
capture_interval="30"
duration="20"
windowed=1
extra_args=()

while [[ $# -gt 0 ]]; do
    case "$1" in
        --runtime-dir)
            runtime_dir="$2"
            shift 2
            ;;
        --save)
            save_name="$2"
            shift 2
            ;;
        --reference-dir)
            reference_dir="$2"
            shift 2
            ;;
        --actual-dir)
            actual_dir="$2"
            shift 2
            ;;
        --diff-dir)
            diff_dir="$2"
            shift 2
            ;;
        --pattern)
            pattern="$2"
            shift 2
            ;;
        --threshold)
            threshold="$2"
            shift 2
            ;;
        --max-differing-percent)
            max_differing_percent="$2"
            shift 2
            ;;
        --frame)
            capture_frame="$2"
            shift 2
            ;;
        --interval)
            capture_interval="$2"
            shift 2
            ;;
        --duration)
            duration="$2"
            shift 2
            ;;
        --fullscreen)
            windowed=0
            shift
            ;;
        --extra-arg)
            extra_args+=("$2")
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "Unknown argument: $1" >&2
            usage >&2
            exit 2
            ;;
    esac
done

if [[ -z "${save_name}" || -z "${reference_dir}" || -z "${actual_dir}" ]]; then
    usage >&2
    exit 2
fi

if [[ ! -d "${reference_dir}" ]]; then
    echo "Missing reference capture directory: ${reference_dir}" >&2
    exit 1
fi

capture_args=(
    --runtime-dir "${runtime_dir}"
    --save "${save_name}"
    --output-dir "${actual_dir}"
    --frame "${capture_frame}"
    --interval "${capture_interval}"
    --duration "${duration}"
)
if [[ "${windowed}" -eq 0 ]]; then
    capture_args+=(--fullscreen)
fi
for arg in "${extra_args[@]}"; do
    capture_args+=(--extra-arg "${arg}")
done

"${script_dir}/capture_generalsmd_save.sh" "${capture_args[@]}"

compare_args=(
    "${reference_dir}"
    "${actual_dir}"
    --pattern "${pattern}"
    --threshold "${threshold}"
    --max-differing-percent "${max_differing_percent}"
)
if [[ -n "${diff_dir}" ]]; then
    compare_args+=(--diff-dir "${diff_dir}")
fi

python3 "${script_dir}/compare_capture_dirs.py" "${compare_args[@]}"
