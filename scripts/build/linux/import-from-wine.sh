#!/usr/bin/env bash
set -euo pipefail

# TheSuperHackers @build bobtista 25/07/2026 Migrate an existing WINE / Proton /
# Steam / Lutris Command & Conquer Generals + Zero Hour install to this native
# build. Copies the game's *.big assets next to this binary so you can play
# without reinstalling from disc. With --with-saves it also imports options.ini
# and your saved games / replays from the WINE prefix's user-data folder.
#
# Usage:
#   ./import-from-wine.sh [--with-saves] [--prefix DIR] [--dry-run] [--force]
#
#   --with-saves   Also copy options.ini, Save/ and Replays/ into the native
#                  user-data folder (~/.local/share/Command and Conquer
#                  Generals Zero Hour Data/).
#   --prefix DIR   Add a custom search root (a WINE prefix, a Steam library, or
#                  the game folder itself). May be given more than once. The
#                  WINEPREFIX environment variable is honored too.
#   --dry-run      Show what would be copied without copying anything.
#   --force        Overwrite .big files that already exist here.

here="$(cd "$(dirname "$0")" && pwd)"
data_dir="${XDG_DATA_HOME:-${HOME}/.local/share}/Command and Conquer Generals Zero Hour Data"

with_saves=0
dry_run=0
force=0
declare -a extra_roots=()

usage()
{
	cat <<'USAGE'
Migrate an existing WINE / Proton / Steam / Lutris Generals + Zero Hour install
to this native build (copies the game's *.big assets next to this binary).

Usage: ./import-from-wine.sh [--with-saves] [--prefix DIR] [--dry-run] [--force]

  --with-saves   Also import options.ini, Save/ and Replays/ from the WINE
                 prefix's user-data folder.
  --prefix DIR   Add a custom search root (may be repeated). WINEPREFIX is
                 honored too.
  --dry-run      Show what would be copied without copying anything.
  --force        Overwrite .big files that already exist here.
USAGE
}

while [ $# -gt 0 ]; do
	case "$1" in
		--with-saves)
			with_saves=1
			;;
		--dry-run)
			dry_run=1
			;;
		--force)
			force=1
			;;
		--prefix)
			shift
			[ $# -gt 0 ] || { echo "--prefix needs a directory" >&2; exit 1; }
			extra_roots+=("$1")
			;;
		-h|--help)
			usage
			exit 0
			;;
		*)
			echo "Unknown option: $1" >&2
			usage
			exit 1
			;;
	esac
	shift
done

# Where existing installs commonly live. Non-existent roots are skipped later.
declare -a roots=(
	"${HOME}/.wine/drive_c/Program Files (x86)/EA Games"
	"${HOME}/.wine/drive_c/Program Files/EA Games"
	"${HOME}/.wine/drive_c/Program Files (x86)/Origin Games"
	"${HOME}/.wine/drive_c/Program Files/Origin Games"
	"${HOME}/.steam/steam/steamapps/common"
	"${HOME}/.local/share/Steam/steamapps/common"
	"${HOME}/.var/app/com.valvesoftware.Steam/.local/share/Steam/steamapps/common"
	"${HOME}/Games"
	"${HOME}/.local/share/lutris"
)
[ -n "${WINEPREFIX:-}" ] && roots+=("${WINEPREFIX}")
roots+=("${extra_roots[@]}")

echo "Native build folder: ${here}"
echo "Searching for existing Generals / Zero Hour .big files..."

# Collect .big files, deduped by lowercased basename (first match wins). A
# retail install keeps the base Generals bigs and the Zero Hour bigs in separate
# folders under one root; the native build needs the union of both.
declare -A seen=()
declare -a found=()
for root in "${roots[@]}"; do
	[ -d "${root}" ] || continue
	while IFS= read -r -d '' big; do
		key="$(basename "${big}")"
		key="${key,,}"
		if [ -z "${seen[${key}]:-}" ]; then
			seen[${key}]=1
			found+=("${big}")
		fi
	done < <(find "${root}" -iname '*.big' -type f -print0 2>/dev/null)
done

if [ "${#found[@]}" -eq 0 ]; then
	echo "No .big files found in the usual locations." >&2
	echo "Point the script at your install with: ./import-from-wine.sh --prefix /path/to/game" >&2
	exit 1
fi

echo "Found ${#found[@]} game archive(s)."
copied=0
skipped=0
for big in "${found[@]}"; do
	dest="${here}/$(basename "${big}")"
	if [ -e "${dest}" ] && [ "${force}" -eq 0 ]; then
		echo "  skip (already here): $(basename "${big}")"
		skipped=$((skipped + 1))
		continue
	fi
	if [ "${dry_run}" -eq 1 ]; then
		echo "  would copy: ${big}"
	else
		cp -f "${big}" "${dest}"
		echo "  copied: $(basename "${big}")"
	fi
	copied=$((copied + 1))
done

if [ "${with_saves}" -eq 1 ]; then
	echo ""
	echo "Importing options.ini and saved games / replays..."
	# The user-data folder lives inside the WINE prefix (drive_c/users/...), not
	# in the game folder, so search the prefixes themselves - including Steam
	# Proton's per-app compatdata prefixes.
	declare -a save_roots=(
		"${HOME}/.wine"
		"${HOME}/.steam/steam/steamapps/compatdata"
		"${HOME}/.local/share/Steam/steamapps/compatdata"
		"${HOME}/.var/app/com.valvesoftware.Steam/.local/share/Steam/steamapps/compatdata"
		"${HOME}/Games"
		"${HOME}/.local/share/lutris"
	)
	[ -n "${WINEPREFIX:-}" ] && save_roots+=("${WINEPREFIX}")
	save_roots+=("${extra_roots[@]}")
	imported_any=0
	for root in "${save_roots[@]}"; do
		[ -d "${root}" ] || continue
		while IFS= read -r -d '' src; do
			# Never re-import our own native user-data folder into itself.
			[ "$(cd "${src}" && pwd)" = "$(cd "${data_dir}" 2>/dev/null && pwd || echo "${data_dir}")" ] && continue
			imported_any=1
			echo "  from: ${src}"
			if [ "${dry_run}" -eq 1 ]; then
				echo "    (dry run, not copying)"
				continue
			fi
			mkdir -p "${data_dir}"
			for item in options.ini Save Replays Maps; do
				if [ -e "${src}/${item}" ]; then
					cp -rf "${src}/${item}" "${data_dir}/"
					echo "    imported ${item}"
				fi
			done
		done < <(find "${root}" -type d -iname "Command and Conquer Generals Zero Hour Data" -print0 2>/dev/null)
	done
	if [ "${imported_any}" -eq 0 ]; then
		echo "  No WINE user-data folder found; nothing to import."
	fi
fi

echo ""
if [ "${dry_run}" -eq 1 ]; then
	echo "Dry run complete. ${copied} archive(s) would be copied, ${skipped} already present."
else
	echo "Done. ${copied} archive(s) copied, ${skipped} already present."
	echo "Launch the game with: ${here}/run.sh -win"
fi
