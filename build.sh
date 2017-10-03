#!/bin/sh

case "$1" in
    "clean" )
        if [ -f Makefile ]; then
            make clean
        fi
        rm -f CMakeCache.txt
        rm -f cmake_install.cmake
        rm -f Makefile
        rm -rf CMakeFiles
        exit 0
        ;;
esac

if command -v cmake >/dev/null 2>&1; then
    cmake -DCMAKE_BUILD_TYPE=release .
    make -j4
else
    echo "Please install cmake."
    exit 1
fi
