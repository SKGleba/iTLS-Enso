#!/usr/bin/env bash -e

PROJECT_DIR=$( cd "$(dirname "${BASH_SOURCE[0]}")" ; pwd -P )

# build builds iTLS-Enso.vpk
function build() {
    cd ${PROJECT_DIR}/plugin
    cmake .
    make
    cd ${PROJECT_DIR}
    make
}

# clean cleans everything build built
function clean() {
    make clean
    cd ${PROJECT_DIR}/plugin
    make clean
    rm CMakeCache.txt
    rm -r CMakeFiles
    rm Makefile
    rm cmake_install.cmake
    cd ${PROJECT_DIR}
}

# run functions
${1} ${@:2}
