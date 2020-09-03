#!/bin/bash
set -eux

SCRIPT_DIR=$(cd $(dirname "$0"); pwd)
cd "$SCRIPT_DIR"

# sudo apt install g++-9-sh4-linux-gnu

# sh4-linux-gnu-addr2line     sh4-linux-gnu-gcc-nm-9      sh4-linux-gnu-objcopy
# sh4-linux-gnu-ar            sh4-linux-gnu-gcc-ranlib-9  sh4-linux-gnu-objdump
# sh4-linux-gnu-as            sh4-linux-gnu-gcov-9        sh4-linux-gnu-ranlib
# sh4-linux-gnu-c++filt       sh4-linux-gnu-gcov-dump-9   sh4-linux-gnu-readelf
# sh4-linux-gnu-cpp-9         sh4-linux-gnu-gcov-tool-9   sh4-linux-gnu-size
# sh4-linux-gnu-elfedit       sh4-linux-gnu-gprof         sh4-linux-gnu-strings
# sh4-linux-gnu-g++-9         sh4-linux-gnu-ld            sh4-linux-gnu-strip
# sh4-linux-gnu-gcc-9         sh4-linux-gnu-ld.bfd
# sh4-linux-gnu-gcc-ar-9      sh4-linux-gnu-nm

wsl ./build.sh