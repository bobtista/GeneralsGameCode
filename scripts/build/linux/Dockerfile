# TheSuperHackers @build bobtista 24/07/2026 Native Linux build image for the
# GeneralsMD SDL3/bgfx client. Ubuntu 24.04 to match the GitHub Actions
# ubuntu-latest runner (same glibc/toolchain), with the X11/Wayland/Vulkan/GL
# and audio dev headers SDL3 and bgfx probe at configure time. FFmpeg is built
# minimally from source (build-ffmpeg-minimal.sh, needs nasm/curl/xz) instead of
# the system package, so it is bundled and portable across distros.
# This is a NATIVE build (gcc), unrelated to the Wine/VC6 cross-build under
# resources/dockerbuild.
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        ninja-build \
        git \
        ca-certificates \
        pkg-config \
        python3 \
        libx11-dev \
        libxext-dev \
        libxrandr-dev \
        libxi-dev \
        libxcursor-dev \
        libxinerama-dev \
        libxfixes-dev \
        libxrender-dev \
        libxss-dev \
        libxtst-dev \
        libxkbcommon-dev \
        libwayland-dev \
        wayland-protocols \
        libdecor-0-dev \
        libgl1-mesa-dev \
        libegl1-mesa-dev \
        libvulkan-dev \
        libfreetype6-dev \
        libfontconfig1-dev \
        libasound2-dev \
        libpulse-dev \
        nasm \
        curl \
        xz-utils \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
