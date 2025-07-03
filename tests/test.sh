#!/usr/bin/env bash

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
PROJECT_ROOT_DIR="${SCRIPT_DIR}/.."

LIBRETRO="OFF"
BUILD_TYPE="Debug"
PIC="ON"
ARCH="arm64"
SYSTEM_NAME="macOS"
RUN_BUILD="ON"
# PASS CLEAN=TRUE to force cleaning (removes previous build)
if [ "${CLEAN}" = "TRUE" ]; then
  echo "Cleaning previous build directory ${PROJECT_ROOT_DIR}/build_for_tests"
  rm -rf "${PROJECT_ROOT_DIR}/build_for_tests"
fi

# PASS CLEAN=TRUE to force cleaning
CLEAN=${CLEAN:-"FALSE"}

# Set VULKAN_SDK environment variable for MoltenVK
export VULKAN_SDK="${HOME}/VulkanSDK/macOS"

# export PATH="/opt/homebrew/bin:$PATH"

# Initialize flags
C_FLAGS="-arch ${ARCH} \
-DTARGET_NO_NIXPROF"

CXX_FLAGS="-arch ${ARCH} \
-DTARGET_NO_NIXPROF"


# Simple helper to configure, build and run the C++ unit-tests (GoogleTest)
# against the SH4 cached-IR executor.
#
# Usage:
#   ./tests/test.sh [build_dir] [extra cmake args ...]
#
# If no build directory is supplied, a default "build/tests" folder is used.
# Any extra arguments are forwarded directly to the cmake configuration step,
# enabling custom generators, compilers, etc.

set -euo pipefail

# Pick build directory from first argument or use default
BUILD_DIR="${1:-${PROJECT_ROOT_DIR}/build_for_tests}"
if [[ $# -gt 0 ]]; then
  shift # remove build dir param so remaining args go to cmake
fi

# Clean the build directory if CLEAN is TRUE
if [[ "${CLEAN}" == "TRUE" ]]; then
    rm -rf "${BUILD_DIR}"
fi

# Configure with CMake
CMAKE_BIN=$(command -v cmake)
if [[ -z "$CMAKE_BIN" ]]; then
  echo "Error: cmake not found in PATH" >&2
  exit 1
fi

"$CMAKE_BIN" -S "${PROJECT_ROOT_DIR}" -B "${BUILD_DIR}" \
      -DCMAKE_TOOLCHAIN_FILE="${SCRIPT_DIR}/macos_clang_toolchain.cmake" \
       -DLIBRETRO=${LIBRETRO} \
      -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
      -DCMAKE_POSITION_INDEPENDENT_CODE=${PIC} \
      -DCMAKE_SYSTEM_NAME=${SYSTEM_NAME} \
      -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
      -DCMAKE_POLICY_DEFAULT_CMP0091=NEW \
      -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
      -DENABLE_SH4_CACHED_IR=ON \
      -DBUILD_TESTING=ON \
      -DENABLE_OPENMP=OFF \
      -DUSE_JIT=OFF \
      -DUSE_BREAKPAD=OFF \
      -DTARGET_NO_NIXPROF=ON \
      -DENABLE_LOG=ON \
      -DCMAKE_C_FLAGS="${C_FLAGS}" \
      -DCMAKE_CXX_FLAGS="${CXX_FLAGS}" \
      -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
       -DUSE_HOST_SDL=OFF \
      "$@"

# Build project (will also build flycast_tests if the target exists)
echo "Build log in ${BUILD_DIR}/build.log"
cmake --build "${BUILD_DIR}" --config ${BUILD_TYPE} -j$(sysctl -n hw.ncpu) > "${BUILD_DIR}/build.log" 2>&1 || {
    tail -n100 "${BUILD_DIR}/build.log"
    echo "Build failed. See ${BUILD_DIR}/build.log"
    exit 1
}
cat "${BUILD_DIR}/build.log" | tail -n20

# --- Run unit tests ---
cd "${BUILD_DIR}"
if [ -x "./flycast_tests" ]; then
    echo "Running flycast_tests binary…"
    ./flycast_tests --gtest_color=yes
    exit $?
fi

# Fallback to CTest if the standalone binary is absent
if command -v ctest >/dev/null 2>&1; then
    echo "Running CTest suites…"
    ctest --output-on-failure -j1
    exit $?
fi

echo "No test executable found." && exit 1
