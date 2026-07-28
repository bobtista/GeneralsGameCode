#!/usr/bin/env bash
set -euo pipefail

# TheSuperHackers @build bobtista 25/07/2026 Migrate an existing WINE / Proton /
# Steam / Lutris Command & Conquer Generals + Zero Hour install to this native
# build. Copies the game's *.big assets next to this binary so you can play
# without reinstalling from disc, along with the loose Data folders the archives
# do not carry. With --with-saves it also imports options.ini and your saved
# games / replays from the WINE prefix's user-data folder.
#
# Usage:
#   ./import-from-wine.sh [--with-saves] [--no-data] [--prefix DIR] [--dry-run]
#                         [--force]
#
#   --with-saves   Also copy options.ini, Save/ and Replays/ into the native
#                  user-data folder (~/.local/share/Command and Conquer
#                  Generals Zero Hour Data/).
#   --no-data      Skip the loose Data folders and copy only the .big archives.
#   --prefix DIR   Add a custom search root (a WINE prefix, a Steam library, or
#                  the game folder itself). May be given more than once. The
#                  WINEPREFIX environment variable is honored too.
#   --dry-run      Show what would be copied without copying anything.
#   --force        Overwrite files that already exist here.

here="$(cd "$(dirname "$0")" && pwd)"
data_dir="${XDG_DATA_HOME:-${HOME}/.local/share}/Command and Conquer Generals Zero Hour Data"

with_saves=0
with_data=1
dry_run=0
force=0
declare -a extra_roots=()

usage()
{
	cat <<'USAGE'
Migrate an existing WINE / Proton / Steam / Lutris Generals + Zero Hour install
to this native build (copies the game's *.big assets and the loose Data folders
next to this binary).

Usage: ./import-from-wine.sh [--with-saves] [--no-data] [--prefix DIR]
                            [--dry-run] [--force]

  --with-saves   Also import options.ini, Save/ and Replays/ from the WINE
                 prefix's user-data folder.
  --no-data      Skip the loose Data folders and copy only the .big archives.
  --prefix DIR   Add a custom search root (may be repeated). WINEPREFIX is
                 honored too.
  --dry-run      Show what would be copied without copying anything.
  --force        Overwrite files that already exist here.
USAGE
}

while [ $# -gt 0 ]; do
	case "$1" in
		--with-saves)
			with_saves=1
			;;
		--no-data)
			with_data=0
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

# TheSuperHackers @build bobtista 28/07/2026 Steam's install root depends on the
# distro and packaging - ~/.steam/steam upstream, ~/.steam/root, and
# ~/.steam/debian-installation on Debian / Ubuntu / Mint - so glob over all of
# them rather than listing a fixed few, and read each libraryfolders.vdf so games
# installed on a second drive are found without --prefix. Echoes one resolved
# steamapps directory per line.
find_steamapps_dirs()
{
	local candidates=()
	local seen_dirs=""
	local d real vdf lib

	shopt -s nullglob
	candidates+=("${HOME}"/.steam/*/steamapps)
	candidates+=("${HOME}"/.local/share/Steam/steamapps)
	candidates+=("${HOME}"/.var/app/com.valvesoftware.Steam/.local/share/Steam/steamapps)
	candidates+=(/home/deck/.local/share/Steam/steamapps)
	# Steam Deck / external drives mount microSD and USB libraries under
	# /run/media (either /run/media/<label> or /run/media/deck/<label>).
	candidates+=(/run/media/*/steamapps /run/media/*/*/steamapps)
	shopt -u nullglob

	# Every library's vdf lists all the other libraries, so a single pass over
	# the globbed candidates reaches drives that were never globbed. Both the
	# current ("path" "/dir") and the older ("1" "/dir") layouts put the library
	# in the last quoted absolute path on the line.
	for d in "${candidates[@]}"; do
		vdf="${d}/libraryfolders.vdf"
		[ -f "${vdf}" ] || continue
		while IFS= read -r lib; do
			[ -d "${lib}/steamapps" ] && candidates+=("${lib}/steamapps")
		done < <(sed -n 's/.*"\(\/[^"]*\)".*/\1/p' "${vdf}")
	done

	for d in "${candidates[@]}"; do
		[ -d "${d}" ] || continue
		# ~/.steam/steam and ~/.steam/root usually symlink to the same tree, so
		# resolve before deduping to avoid scanning it several times over.
		real="$(cd "${d}" && pwd -P)"
		case "${seen_dirs}" in
			*"|${real}|"*)
				continue
				;;
		esac
		seen_dirs="${seen_dirs}|${real}|"
		printf '%s\n' "${real}"
	done
}

# Where existing installs commonly live. Non-existent roots are skipped later.
declare -a roots=(
	"${HOME}/.wine/drive_c/Program Files (x86)/EA Games"
	"${HOME}/.wine/drive_c/Program Files/EA Games"
	"${HOME}/.wine/drive_c/Program Files (x86)/Origin Games"
	"${HOME}/.wine/drive_c/Program Files/Origin Games"
	"${HOME}/Games"
	"${HOME}/.local/share/lutris"
)
while IFS= read -r steamapps; do
	roots+=("${steamapps}/common")
done < <(find_steamapps_dirs)
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

data_copied=0
data_found=0
if [ "${with_data}" -eq 1 ]; then
	echo ""
	echo "Importing loose game data (scripts, cursors, movies)..."
	# TheSuperHackers @build bobtista 29/07/2026 No retail archive holds a .scb,
	# .ani or .bik entry, so the skirmish / multiplayer scripts, the mouse cursors
	# and the movies exist only as loose files beside the archives and have to be
	# brought over too. Data/INI and Data/<language>/*.csf are deliberately left
	# behind: those live inside the .big files, and loose copies would shadow the
	# archived ones (including our own Data/INI/Bgfx.ini).
	declare -a zh_dirs=()
	declare -a base_dirs=()
	seen_game_dirs=""
	for big in "${found[@]}"; do
		game_dir="$(cd "$(dirname "${big}")" && pwd -P)"
		case "${seen_game_dirs}" in
			*"|${game_dir}|"*)
				continue
				;;
		esac
		seen_game_dirs="${seen_game_dirs}|${game_dir}|"
		shopt -s nullglob
		data_roots=("${game_dir}"/[Dd]ata)
		zh_archives=("${game_dir}"/*[Zz][Hh].big)
		shopt -u nullglob
		[ "${#data_roots[@]}" -gt 0 ] || continue
		# Where a Zero Hour folder and a base Generals folder both carry a file,
		# the Zero Hour copy has to win - the same precedence the engine gives
		# the *ZH.big archives - so import those folders first.
		if [ "${#zh_archives[@]}" -gt 0 ]; then
			zh_dirs+=("${game_dir}")
		else
			base_dirs+=("${game_dir}")
		fi
	done

	for game_dir in "${zh_dirs[@]}" "${base_dirs[@]}"; do
		shopt -s nullglob
		# Scripts and Cursors sit directly under Data; movies are in Data/Movies
		# plus Data/<language>/Movies for the localized campaign and challenge
		# videos.
		sources=(
			"${game_dir}"/[Dd]ata/[Ss]cripts
			"${game_dir}"/[Dd]ata/[Cc]ursors
			"${game_dir}"/[Dd]ata/[Mm]ovies
			"${game_dir}"/[Dd]ata/*/[Mm]ovies
		)
		shopt -u nullglob
		for src in "${sources[@]}"; do
			[ -d "${src}" ] || continue
			rel="${src#"${game_dir}/"}"
			dest="${here}/${rel}"
			# Copied file by file rather than with cp -rn: coreutils 9.3 warns
			# that -n is non-portable, and --update=none is too new to rely on.
			# Skipping files that are already here also means a base Generals
			# folder can never overwrite what Zero Hour put down first.
			dir_copied=0
			dir_skipped=0
			while IFS= read -r -d '' file; do
				target="${dest}/${file#"${src}/"}"
				if [ -e "${target}" ] && [ "${force}" -eq 0 ]; then
					dir_skipped=$((dir_skipped + 1))
					continue
				fi
				if [ "${dry_run}" -eq 0 ]; then
					mkdir -p "$(dirname "${target}")"
					cp -f "${file}" "${target}"
				fi
				dir_copied=$((dir_copied + 1))
			done < <(find "${src}" -type f -print0 2>/dev/null)
			[ "${dir_copied}" -gt 0 ] || [ "${dir_skipped}" -gt 0 ] || continue
			data_found=1
			if [ "${dir_copied}" -eq 0 ]; then
				echo "  up to date: ${rel} - ${dir_skipped} file(s) already present"
			elif [ "${dry_run}" -eq 1 ]; then
				echo "  would import: ${rel} - ${dir_copied} file(s), ${dir_skipped} already present"
			else
				echo "  imported: ${rel} - ${dir_copied} file(s), ${dir_skipped} already present"
			fi
			data_copied=$((data_copied + dir_copied))
		done
	done

	if [ "${data_found}" -eq 0 ]; then
		echo "  No loose Data folder found next to the archives."
		echo "  Skirmish scripts, cursors and movies are not inside the .big files;" >&2
		echo "  without them skirmish AI, the game cursors and the videos will be missing." >&2
	fi
fi

if [ "${with_saves}" -eq 1 ]; then
	echo ""
	echo "Importing options.ini and saved games / replays..."
	# The user-data folder lives inside the WINE prefix (drive_c/users/...), not
	# in the game folder, so search the prefixes themselves - including Steam
	# Proton's per-app compatdata prefixes.
	declare -a save_roots=(
		"${HOME}/.wine"
		"${HOME}/Games"
		"${HOME}/.local/share/lutris"
	)
	while IFS= read -r steamapps; do
		save_roots+=("${steamapps}/compatdata")
	done < <(find_steamapps_dirs)
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
	echo "Dry run complete. ${copied} archive(s) would be copied, ${skipped} already present,"
	echo "${data_copied} loose data file(s) would be imported."
else
	echo "Done. ${copied} archive(s) copied, ${skipped} already present,"
	echo "${data_copied} loose data file(s) imported."
	echo "Launch the game with: ${here}/run.sh -win"
fi
