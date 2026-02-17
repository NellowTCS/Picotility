#!/bin/bash
# Run cart tests

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

if [ ! -f "build/test_runner" ]; then
    mkdir -p build
    cd build
    cmake .. > /dev/null 2>&1
    make -j4 > /dev/null 2>&1
    cd ..
fi

./build/test_runner "$@"
