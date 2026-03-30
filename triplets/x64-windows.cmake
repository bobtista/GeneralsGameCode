# Include the default x64-windows triplet
set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE dynamic)

# Exclude compiler version from ABI hash so that Visual Studio version updates
# on GitHub runner images do not invalidate the binary cache for host tools.
# These are build-only tools (cmake, meson, pkgconf) whose ABIs propagate into
# target package hashes; compiler version bumps do not affect their output.
set(VCPKG_DISABLE_COMPILER_TRACKING ON)
