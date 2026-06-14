#!/usr/bin/env bash
# TheSuperHackers @feature bobtista 14/06/2026
# Cross-platform deterministic simulation-math parity check.
#
# Runs `<binary> -mathCrcCheck`, reads the two CRCs it emits, and compares them against the
# canonical reference (scripts/determinism/mathcrc-reference.txt). Identical CRCs confirm the
# deterministic-math path matches the reference architecture; a SimulationMathCrc mismatch is a
# lockstep-breaking floating-point divergence.
#
# Usage:
#   check-mathcrc.sh <path-to-generalszh> [--allow-double-divergence] [--reference <file>]
#
#   --allow-double-divergence   Treat a SimulationMathCrcDouble mismatch as a warning, not a
#                               failure. Use for 32-bit x86 (x87 _PC_24) builds, which clamp
#                               double precision and may legitimately differ on the double probe.
#   --reference <file>          Override the reference file.
#
# Exit status: 0 = parity OK, 1 = divergence, 2 = usage/run error.
# Works wherever bash runs, including Git Bash on Windows CI runners.

set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
reference="${script_dir}/mathcrc-reference.txt"
allow_double_divergence=0
binary=""

while [ $# -gt 0 ]; do
	case "$1" in
		--allow-double-divergence) allow_double_divergence=1; shift ;;
		--reference) reference="$2"; shift 2 ;;
		-h|--help) sed -n '2,20p' "${BASH_SOURCE[0]}"; exit 0 ;;
		*) binary="$1"; shift ;;
	esac
done

if [ -z "${binary}" ] || [ ! -x "${binary}" ]; then
	echo "error: pass an executable game binary (got '${binary:-<none>}')" >&2
	exit 2
fi
if [ ! -f "${reference}" ]; then
	echo "error: reference file not found: ${reference}" >&2
	exit 2
fi

extract() { # field, text -> hex value (uppercased), empty if absent
	printf '%s\n' "$2" | grep -ioE "$1 *= *[0-9A-Fa-f]{8}" | head -1 | grep -oE '[0-9A-Fa-f]{8}$' | tr 'a-f' 'A-F'
}

ref_text="$(cat "${reference}")"
ref_single="$(extract SimulationMathCrc "${ref_text}")"
ref_double="$(extract SimulationMathCrcDouble "${ref_text}")"

# -mathCrcCheck prints to stdout and writes SimulationMathCrc.txt; capture both and prefer the file.
run_dir="$(dirname "${binary}")"
run_out="$(cd "${run_dir}" && "./$(basename "${binary}")" -mathCrcCheck 2>/dev/null || true)"
artifact="${run_dir}/SimulationMathCrc.txt"
got_text="${run_out}"
if [ -f "${artifact}" ]; then
	got_text="$(cat "${artifact}")"
fi

got_single="$(extract SimulationMathCrc "${got_text}")"
got_double="$(extract SimulationMathCrcDouble "${got_text}")"

if [ -z "${got_single}" ] || [ -z "${got_double}" ]; then
	echo "error: could not read CRCs from the binary (stdout or SimulationMathCrc.txt)." >&2
	echo "       build must emit them; pure Release with no console can only use the artifact file." >&2
	exit 2
fi

status=0
report() { # label, expected, got, fatal(1/0)
	if [ "$2" = "$3" ]; then
		printf '  %-26s %s  OK\n' "$1" "$3"
	elif [ "$4" = "1" ]; then
		printf '  %-26s %s  MISMATCH (expected %s)\n' "$1" "$3" "$2"; status=1
	else
		printf '  %-26s %s  differs (expected %s) [tolerated]\n' "$1" "$3" "$2"
	fi
}

echo "deterministic math parity vs ${reference}:"
report "SimulationMathCrc"       "${ref_single}" "${got_single}" 1
report "SimulationMathCrcDouble" "${ref_double}" "${got_double}" "$([ "${allow_double_divergence}" = "1" ] && echo 0 || echo 1)"

if [ "${status}" -eq 0 ]; then
	echo "PARITY OK"
else
	echo "PARITY FAILED - floating-point divergence breaks cross-platform lockstep" >&2
fi
exit "${status}"
