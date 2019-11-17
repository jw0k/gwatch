#!/usr/bin/env bash
#
# Environment variables:
#
# SOURCE_DIR: Set to the directory of the libgit2 source (optional)
#     If not set, it will be derived relative to this script.

set -e

SOURCE_DIR=${SOURCE_DIR:-$( cd "$( dirname "${BASH_SOURCE[0]}" )" && dirname $( pwd ) )}
BUILD_DIR=$(pwd)
CC=${CC:-cc}

indent() { sed "s/^/    /"; }

echo "Source directory: ${SOURCE_DIR}"
echo "Build directory:  ${BUILD_DIR}"
echo ""
echo "Operating system version:"
uname -a 2>&1 | indent
echo "CMake version:"
cmake --version 2>&1 | indent
echo "Compiler version:"
$CC --version 2>&1 | indent
echo ""

echo "##############################################################################"
echo "## Configuring build environment"
echo "##############################################################################"

echo cmake ${SOURCE_DIR} -DENABLE_WERROR=ON -DBUILD_EXAMPLES=ON ${CMAKE_OPTIONS}
cmake ${SOURCE_DIR} -DENABLE_WERROR=ON -DBUILD_EXAMPLES=ON ${CMAKE_OPTIONS}

echo ""
echo "##############################################################################"
echo "## Building libgit2"
echo "##############################################################################"

cmake --build .
