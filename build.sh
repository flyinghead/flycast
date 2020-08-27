#!/bin/bash

set -eux

cd $(dirname "$0")

./build.bat $@
