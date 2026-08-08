#!/usr/bin/env bash
# The conda-forge clang toolchain does not know where the macOS SDK lives, so
# point the compiler, CMake, and linker at the system SDK explicitly. This
# mirrors the macOS conda-clang setup in .github/workflows/cmake.yml; CMake
# picks up SDKROOT, CFLAGS/CXXFLAGS, and LDFLAGS at first configure.
if command -v xcrun >/dev/null 2>&1; then
  SDKROOT="$(xcrun --sdk macosx --show-sdk-path)"
  export SDKROOT
  export CONDA_BUILD_SYSROOT="${SDKROOT}"
  export CFLAGS="${CFLAGS:+${CFLAGS} }--sysroot=${SDKROOT}"
  export CXXFLAGS="${CXXFLAGS:+${CXXFLAGS} }--sysroot=${SDKROOT}"
  export LDFLAGS="${LDFLAGS:+${LDFLAGS} }-fuse-ld=lld --sysroot=${SDKROOT}"
else
  echo "warning: xcrun not found; macOS SDK setup skipped (install Xcode command line tools)" 1>&2
fi
