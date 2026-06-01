#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'USAGE'
Usage:
  capture_generalsmd_save.sh --save 00000051.sav --output-dir .context/captures/case [options]

Options:
  --runtime-dir DIR   Deployed GeneralsZH directory. Default: $GGC_MACOS_RUNTIME_DIR or ~/TheSuperHackers/GeneralsZH
  --save NAME         Save basename to pass to -loadsave, for example 00000051.sav.
  --output-dir DIR    Directory for bgfx BMP captures.
  --frame N           First bgfx frame to request. Default: 120.
  --interval N        Screenshot interval in bgfx frames. Default: 30.
  --duration SECONDS  Wall-clock run duration before graceful stop. Default: 20.
  --fullscreen        Run without -win.
  --extra-arg ARG     Extra argument passed through to generalszh. Repeatable.
  -h, --help          Show this help.
USAGE
}

runtime_dir="${GGC_MACOS_RUNTIME_DIR:-${HOME}/TheSuperHackers/GeneralsZH}"
save_name=""
output_dir=""
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
        --output-dir)
            output_dir="$2"
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

if [[ -z "${save_name}" || -z "${output_dir}" ]]; then
    usage >&2
    exit 2
fi

if [[ ! -x "${runtime_dir}/run.sh" ]]; then
    echo "Missing executable runtime wrapper: ${runtime_dir}/run.sh" >&2
    exit 1
fi

mkdir -p "${output_dir}"
output_dir="$(cd "${output_dir}" && pwd)"
base_path="${output_dir%/}/${save_name%.sav}"
find "${output_dir}" -maxdepth 1 -type f -name "${save_name%.sav}.*.bmp" -delete

args=(-nologo -quickstart -loadsave "${save_name}" -bgfxScreenshotAfter "${capture_frame}" "${base_path}")
if [[ "${windowed}" -eq 1 ]]; then
    args+=(-win)
fi
if [[ ${#extra_args[@]} -gt 0 ]]; then
    args+=("${extra_args[@]}")
fi

echo "Capturing ${save_name} to ${base_path}.NNNNNN.bmp"
(
    cd "${runtime_dir}"
    GGC_BGFX_SCREENSHOT_INTERVAL="${capture_interval}" ./run.sh "${args[@]}"
) &
pid=$!

sleep "${duration}"
if kill -0 "${pid}" 2>/dev/null; then
    kill -TERM "${pid}" 2>/dev/null || true
    wait "${pid}" 2>/dev/null || true
else
    wait "${pid}"
fi

captures=("${base_path}".*.bmp)
if [[ ! -e "${captures[0]}" ]]; then
    echo "No captures produced for ${save_name}" >&2
    exit 1
fi

printf '%s\n' "${captures[@]}"
