#!/bin/bash

# Default values
BUILD_DIR="build/ios-arm64"
LIBRETRO="ON"
BUILD_TYPE="Release"
PIC="ON"
ARCH="arm64"
IOS_MIN_VERSION="15.0"
IOS="ON"
SYSTEM_NAME="iOS"
RUN_BUILD="ON"
# Dynarec type: "jitless" (default), "jit", or "none"
DYNAREC_TYPE=${DYNAREC_TYPE:-"jitless"}

# Set JIT flags based on dynarec type
if [ "$DYNAREC_TYPE" = "jitless" ]; then
    USE_JIT="OFF"
    NO_JIT="ON"
    echo "🔧 Building with JITLESS dynarec"
else
    USE_JIT="ON"
    NO_JIT="OFF"
    echo "🔧 Building with NO dynarec (interpreter only)"
fi

export VULKAN_SDK="${HOME}/VulkanSDK/macOS"

# Initialize FMV-optimized flags (Core Clang optimizations only)
C_FLAGS="-arch ${ARCH} \
-DIOS \
-DNO_JIT=${NO_JIT} \
-DUSE_JIT=${USE_JIT} \
-DTARGET_NO_NIXPROF \
-miphoneos-version-min=${IOS_MIN_VERSION} \
-fdata-sections \
-ffast-math \
-ffunction-sections \
-finline-functions \
-flto=thin \
-fno-strict-aliasing \
-fomit-frame-pointer \
-fpermissive \
-ftree-vectorize \
-funsafe-math-optimizations \
-fvectorize \
-march=armv8-a+simd+crc+crypto+dotprod+fp16 \
-mcpu=apple-a14 \
-mtune=apple-a14 \
-O3 \
-Ofast \
-fno-math-errno \
-ffinite-math-only \
-fno-signed-zeros \
-fno-trapping-math \
-freciprocal-math \
-ffp-contract=fast \
-funroll-loops \
-DARM_NEON \
-DHAVE_NEON \
-DENABLE_NEON_OPT \
-DFMV_OPTIMIZED"

CXX_FLAGS="-arch ${ARCH} \
-DIOS \
-DNO_JIT=${NO_JIT} \
-DUSE_JIT=${USE_JIT} \
-miphoneos-version-min=${IOS_MIN_VERSION} \
-DTARGET_NO_NIXPROF \
-fdata-sections \
-ffast-math \
-ffunction-sections \
-finline-functions \
-flto=thin \
-fno-strict-aliasing \
-fomit-frame-pointer \
-fpermissive \
-ftree-vectorize \
-funsafe-math-optimizations \
-fvectorize \
-march=armv8-a+simd+crc+crypto+dotprod+fp16 \
-mcpu=apple-a14 \
-mtune=apple-a14 \
-O3 \
-Ofast \
-fno-math-errno \
-ffinite-math-only \
-fno-signed-zeros \
-fno-trapping-math \
-freciprocal-math \
-ffp-contract=fast \
-funroll-loops \
-std=c++17 \
-DARM_NEON \
-DHAVE_NEON \
-DENABLE_NEON_OPT \
-DFMV_OPTIMIZED"

# Add a function to display usage information
print_usage() {
  echo "iOS/tvOS Build Script"
  echo "====================="
  echo
  echo "Usage: $0 [options]"
  echo
  echo "Options:"
  echo "  --build-dir=DIR           Set build directory (default: build/ios-arm64)"
  echo "  --libretro=ON|OFF         Enable/disable LIBRETRO (default: ON)"
  echo "  --build-type=TYPE         Set build type (default: Release)"
  echo "  --pic=ON|OFF              Enable/disable position independent code (default: ON)"
  echo "  --arch=ARCH               Set architecture (default: arm64)"
  echo "  --ios-min=VERSION         Set iOS minimum version (default: 15.0)"
  echo "  --ios=ON|OFF              Enable/disable iOS (default: ON)"
  echo "  --system-name=NAME        Set system name (default: iOS)"
  echo "  --append-c-flags=FLAGS    Append flags to CMAKE_C_FLAGS"
  echo "  --append-cxx-flags=FLAGS  Append flags to CMAKE_CXX_FLAGS"
  echo "  --append-linker-flags=FLAGS Append flags to CMAKE_EXE_LINKER_FLAGS"
  echo "  --run-build=ON|OFF        Run make after configuration (default: OFF)"
  echo "  --help                    Display this help message"
  echo
  echo "Examples:"
  echo "  $0 --build-type=Debug"
  echo "  $0 --arch=arm64e --ios-min=16.0"
  echo "  $0 --append-c-flags=\"-DDEBUG -O3\""
  echo "  $0 --append-cxx-flags=\"-std=c++17\" --append-linker-flags=\"-framework Metal\""
  echo "  $0 --system-name=tvOS --build-dir=build/tvos-arm64"
  echo "  $0 --run-build=ON --build-type=Release"
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
  case $1 in
    --build-dir=*)
      BUILD_DIR="${1#*=}"
      shift
      ;;
    --libretro=*)
      LIBRETRO="${1#*=}"
      shift
      ;;
    --build-type=*)
      BUILD_TYPE="${1#*=}"
      shift
      ;;
    --pic=*)
      PIC="${1#*=}"
      shift
      ;;
    --arch=*)
      ARCH="${1#*=}"
      # Update arch in flags
      C_FLAGS=$(echo "$C_FLAGS" | sed "s/-arch [^ ]*/-arch ${1#*=}/")
      CXX_FLAGS=$(echo "$CXX_FLAGS" | sed "s/-arch [^ ]*/-arch ${1#*=}/")
      shift
      ;;
    --ios-min=*)
      IOS_MIN_VERSION="${1#*=}"
      # Update min version in flags
      C_FLAGS=$(echo "$C_FLAGS" | sed "s/-miphoneos-version-min=[^ ]*/-miphoneos-version-min=${1#*=}/")
      CXX_FLAGS=$(echo "$CXX_FLAGS" | sed "s/-miphoneos-version-min=[^ ]*/-miphoneos-version-min=${1#*=}/")
      shift
      ;;
    --ios=*)
      IOS="${1#*=}"
      shift
      ;;
    --system-name=*)
      SYSTEM_NAME="${1#*=}"
      shift
      ;;
    --append-c-flags=*)
      C_FLAGS="$C_FLAGS ${1#*=}"
      shift
      ;;
    --append-cxx-flags=*)
      CXX_FLAGS="$CXX_FLAGS ${1#*=}"
      shift
      ;;
    --append-linker-flags=*)
      LINKER_FLAGS="$LINKER_FLAGS ${1#*=}"
      shift
      ;;
    --run-build=*)
      RUN_BUILD="${1#*=}"
      shift
      ;;
    --help)
      print_usage
      exit 0
      ;;
    *)
      echo "Unknown option: $1"
      echo "Use --help for usage information"
      exit 1
      ;;
  esac
done

# Add a check for no arguments to show help
if [ $# -eq 0 ]; then
  echo "Running with default settings. Use --help for more options."
  echo
fi

# FMV-optimized linker flags for maximum performance (Simple and reliable)
FMV_LINKER_FLAGS="-Wl,-dead_strip \
-Wl,-dead_strip_dylibs \
-Wl,-merge_zero_fill_sections \
-flto=thin"

# Combine with user linker flags
if [ -n "$LINKER_FLAGS" ]; then
  COMBINED_LINKER_FLAGS="$FMV_LINKER_FLAGS $LINKER_FLAGS"
else
  COMBINED_LINKER_FLAGS="$FMV_LINKER_FLAGS"
fi

# Build the cmake command
CMAKE_CMD="cmake -B ${BUILD_DIR} \
  -DLIBRETRO=${LIBRETRO} \
  -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
  -DCMAKE_POSITION_INDEPENDENT_CODE=${PIC} \
  -DCMAKE_C_FLAGS=\"${C_FLAGS}\" \
  -DCMAKE_CXX_FLAGS=\"${CXX_FLAGS}\" \
  -DCMAKE_EXE_LINKER_FLAGS=\"${COMBINED_LINKER_FLAGS}\" \
  -DCMAKE_SHARED_LINKER_FLAGS=\"${COMBINED_LINKER_FLAGS}\" \
  -DIOS=${IOS} \
  -DCMAKE_SYSTEM_NAME=${SYSTEM_NAME} \
  -DENABLE_SH4_CACHED_IR=OFF \
  -DNO_JIT=${NO_JIT} \
  -DUSE_JIT=${USE_JIT} \
  -DENABLE_LTO=ON \
  -DUSE_LINK_TIME_OPTIMIZATION=ON \
  -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5"

# Linker flags are now handled in the FMV optimization block above

# Print the command for verification
echo "Executing: $CMAKE_CMD"

# Execute the command
eval "$CMAKE_CMD"

# Check if cmake was successful
if [ $? -eq 0 ]; then
  echo "CMake configuration successful!"

  # Run make if requested
  if [ "${RUN_BUILD}" = "ON" ]; then
    echo "Running make in ${BUILD_DIR}..."
    cmake --build ${BUILD_DIR} -- -j$(sysctl -n hw.ncpu)

    if [ $? -eq 0 ]; then
      echo "Build successful!"
    else
      echo "Build failed!"
      exit 1
    fi
  fi
else
  echo "CMake configuration failed!"
  exit 1
fi
