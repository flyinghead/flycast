#!/bin/bash
set -eux

SCRIPT_DIR=$(cd $(dirname "$0"); pwd)
cd "$SCRIPT_DIR"

wsl ./build.sh