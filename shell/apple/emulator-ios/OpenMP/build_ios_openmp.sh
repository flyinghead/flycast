#!/bin/bash
usage() { echo "Usage: $0 {-d | --disableSimulator}" 1>&2; exit 1; }

while [ "${1:-}" != "" ]; do
    case "$1" in
        "-d" | "--disableSimulator")
            disableSimulator=true
            ;;
        "-h" | "--help")
            usage
            exit
            ;;
    esac
    shift
done

OMP_VER=14.0.6
DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
SRC_DIR="${DIR}/openmp-${OMP_VER}.src"

# Download OpenMP
(cd ${DIR} && curl -OL "https://github.com/llvm/llvm-project/releases/download/llvmorg-${OMP_VER}/openmp-${OMP_VER}.src.tar.xz")
(cd ${DIR} && tar -xf ${DIR}/openmp-${OMP_VER}.src.tar.xz openmp-${OMP_VER}.src)

# Build iOS
mkdir -p ${SRC_DIR}/build
cmake -B ${SRC_DIR}/build -S ${SRC_DIR} \
    -DCMAKE_TOOLCHAIN_FILE=${DIR}/ios.toolchain.cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=${SRC_DIR}/build/install \
    -DIOS_PLATFORM=OS -DENABLE_BITCODE=1 -DENABLE_ARC=0 -DENABLE_VISIBILITY=0 -DIOS_ARCH="arm64;arm64e" \
    -DPERL_EXECUTABLE=$(which perl) \
    -DLIBOMP_ENABLE_SHARED=OFF -DLIBOMP_OMPT_SUPPORT=OFF -DLIBOMP_USE_HWLOC=OFF
cmake --build ${SRC_DIR}/build -j 3
cmake --build ${SRC_DIR}/build --target install
FRAMEWORK_DIR="${SRC_DIR}/build/install/OpenMP.framework"
mkdir -p "${FRAMEWORK_DIR}/Versions/A/Headers"
ln -sfh A "${FRAMEWORK_DIR}/Versions/Current"
ln -sfh Versions/Current/Headers "${FRAMEWORK_DIR}/Headers"
ln -sfh "Versions/Current/libomp.a" \
             "${FRAMEWORK_DIR}/OpenMP"
cp ${SRC_DIR}/build/install/lib/libomp.a "${FRAMEWORK_DIR}/Versions/A/libomp.a"
cp ${SRC_DIR}/build/install/include/* "${FRAMEWORK_DIR}/Versions/A/Headers"


# Build Simulator
if [ -z "$disableSimulator" ]
then
    mkdir -p ${SRC_DIR}/build-simulator
    cmake -B ${SRC_DIR}/build-simulator -S ${SRC_DIR} \
        -DCMAKE_TOOLCHAIN_FILE=${DIR}/ios.toolchain.cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=${SRC_DIR}/build-simulator/install \
        -DIOS_PLATFORM=SIMULATOR -DENABLE_BITCODE=1 -DENABLE_ARC=0 -DENABLE_VISIBILITY=0 -DIOS_ARCH="x86_64;arm64" \
        -DPERL_EXECUTABLE=$(which perl) \
        -DLIBOMP_ENABLE_SHARED=OFF -DLIBOMP_OMPT_SUPPORT=OFF -DLIBOMP_USE_HWLOC=OFF
    cmake --build ${SRC_DIR}/build-simulator -j 3
    cmake --build ${SRC_DIR}/build-simulator --target install
    FRAMEWORK_SIM_DIR="${SRC_DIR}/build-simulator/install/OpenMP.framework"
    mkdir -p "${FRAMEWORK_SIM_DIR}/Versions/A/Headers"
    ln -sfh A "${FRAMEWORK_SIM_DIR}/Versions/Current"
    ln -sfh Versions/Current/Headers "${FRAMEWORK_SIM_DIR}/Headers"
    ln -sfh "Versions/Current/libomp.a" \
                "${FRAMEWORK_SIM_DIR}/OpenMP"
    cp ${SRC_DIR}/build-simulator/install/lib/libomp.a "${FRAMEWORK_SIM_DIR}/Versions/A/libomp.a"
    cp ${SRC_DIR}/build-simulator/install/include/* "${FRAMEWORK_SIM_DIR}/Versions/A/Headers"

    ARG_SIM="-framework $FRAMEWORK_SIM_DIR "
fi

# Create XCFramework
rm -rf "${DIR}/OpenMP.xcframework"
xcodebuild -create-xcframework -framework $FRAMEWORK_DIR $ARG_SIM -output "${DIR}/OpenMP.xcframework"

# Clean up
rm -rf "${DIR}/openmp-${OMP_VER}"*

