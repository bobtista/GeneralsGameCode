#!/usr/bin/env bash
set -euo pipefail

# TheSuperHackers @build bobtista 25/07/2026 Build a minimal, self-contained
# FFmpeg for the native Linux build. Ubuntu's system FFmpeg pulls a ~150MB
# closure of codec + crypto libraries and ties the binary to one FFmpeg major
# version (libavcodec.so.60), which is absent on Arch / SteamOS. This builds a
# stripped FFmpeg (only the decoders the game needs, no external codec libs) so
# the ~6MB of shared libs can be bundled and the binary runs on any modern
# distro, including the Steam Deck.
#
# Usage: build-ffmpeg-minimal.sh [install_prefix]
# Point the build at it with: PKG_CONFIG_PATH=<prefix>/lib/pkgconfig
#
# Requires: a C toolchain, make, pkg-config, nasm (x86 asm), curl, xz.

prefix="${1:-${GGC_FFMPEG_PREFIX:-${PWD}/ffmpeg-min}}"
version="${FFMPEG_VERSION:-6.1.2}"
sha256="${FFMPEG_SHA256:-3b624649725ecdc565c903ca6643d41f33bd49239922e45c9b1442c63dca4e38}"

# Cache hit: a previous build already installed here.
if [ -f "${prefix}/lib/pkgconfig/libavcodec.pc" ]; then
	echo "Minimal FFmpeg already present at ${prefix}; skipping build."
	exit 0
fi

work="$(mktemp -d)"
trap 'rm -rf "${work}"' EXIT

tarball="${work}/ffmpeg-${version}.tar.xz"
echo "Downloading ffmpeg ${version}..."
curl -fsSL "https://ffmpeg.org/releases/ffmpeg-${version}.tar.xz" -o "${tarball}"
echo "${sha256}  ${tarball}" | sha256sum -c -

tar -xf "${tarball}" -C "${work}"
src="${work}/ffmpeg-${version}"
build="${work}/build"
mkdir -p "${build}"

# --disable-everything then re-enable only the file protocol and the demuxers /
# decoders the game's cutscenes use (Bink primarily, plus common native codecs).
# No --enable-lib* means zero external codec dependencies.
cd "${build}"
"${src}/configure" \
	--prefix="${prefix}" \
	--enable-shared --disable-static \
	--enable-pic --disable-debug \
	--disable-programs --disable-doc --disable-htmlpages --disable-manpages \
	--disable-avdevice --disable-avfilter --disable-postproc --disable-network \
	--disable-everything \
	--enable-protocol=file \
	--enable-demuxer=bink,smacker,mov,avi,matroska,asf,mpegps,mpegts,ogg,wav,flv,h264,hevc,m4v \
	--enable-decoder=bink,binkaudio_dct,binkaudio_rdft,smackaud,smacker,h264,hevc,mpeg1video,mpeg2video,mpeg4,msmpeg4v1,msmpeg4v2,msmpeg4v3,wmv1,wmv2,wmv3,vc1,vp6,vp6a,vp6f,vp8,vp9,theora,flv,vorbis,aac,ac3,mp3,pcm_s16le,pcm_u8 \
	--enable-parser=h264,hevc,mpeg4video,mpegvideo,vp8,vp9,vorbis,aac,ac3

make -j"$(nproc)"
make install

echo "Minimal FFmpeg installed to ${prefix} ($(du -shc "${prefix}"/lib/*.so.* 2>/dev/null | tail -1 | cut -f1))"
