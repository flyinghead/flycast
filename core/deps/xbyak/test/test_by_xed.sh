#!/bin/sh

set -e
XED=${XED:=xed}
CXX=${CXX:=g++}
PYTHON=${PYTHON:=python3}
echo $XED

if [ $# -ne 1 ]; then
  echo "./test_by_xed.sh <xbyak-cpp>"
  exit 1
fi

TARGET=$1

CFLAGS="-Wall -Wextra -I ../"

echo "test:" $TARGET
cp $TARGET tmp.cpp
$CXX $CFLAGS test_by_xed.cpp -o test_by_xed
./test_by_xed
$XED -64 -ir bin > out.txt
$PYTHON test_by_xed.py $TARGET out.txt

