# TheSuperHackers @build bobtista 24/07/2026 Native Linux build image for the
# GeneralsMD SDL3/bgfx client. Debian 12 (cmake 3.25, meets the project minimum)
# with the X11/Wayland/Vulkan/GL, audio, and FFmpeg dev headers SDL3, bgfx and
# FindFFMPEG probe at configure time. This is a NATIVE build (gcc), unrelated to
# the Wine/VC6 cross-build under resources/dockerbuild.
FROM debian:12-slim

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
        libasound2-dev \
        libpulse-dev \
        libavcodec-dev \
        libavformat-dev \
        libavutil-dev \
        libswscale-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
