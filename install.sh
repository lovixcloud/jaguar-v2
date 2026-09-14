#!/usr/bin/env bash
set -e

PREFIX="${PREFIX:-/usr/local}"

echo "Building Jaguar Compiler Toolchain..."
make clean
make

EXE_NAME="jag"
if [ -f "jag.exe" ]; then
    EXE_NAME="jag.exe"
fi

echo "Installing ${EXE_NAME} binary and runtime library to ${PREFIX}..."
mkdir -p "${PREFIX}/bin"
mkdir -p "${PREFIX}/lib"
mkdir -p "${PREFIX}/include/jaguar"

cp "${EXE_NAME}" "${PREFIX}/bin/${EXE_NAME}"
chmod 0755 "${PREFIX}/bin/${EXE_NAME}" 2>/dev/null || true

if [ -f "libjagrt.a" ]; then
    cp libjagrt.a "${PREFIX}/lib/libjagrt.a"
fi

cp -r runtime/* "${PREFIX}/include/jaguar/" 2>/dev/null || true

echo "Jaguar successfully installed! Run '${EXE_NAME} --version' to verify."
