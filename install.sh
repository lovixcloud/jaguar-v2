#!/usr/bin/env bash
set -e

PREFIX="${PREFIX:-/usr/local}"

echo "Building Jaguar Compiler Toolchain..."
make clean
make

echo "Installing jag binary and runtime library to ${PREFIX}..."
install -d "${PREFIX}/bin"
install -d "${PREFIX}/lib"
install -d "${PREFIX}/include/jaguar"

install -m 0755 jag "${PREFIX}/bin/jag"
install -m 0644 libjagrt.a "${PREFIX}/lib/libjagrt.a"
cp -r runtime/* "${PREFIX}/include/jaguar/"

echo "Jaguar successfully installed! Run 'jag --version' to verify."
