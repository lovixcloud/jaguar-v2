#!/usr/bin/env bash
set -e

PREFIX="${PREFIX:-/usr/local}"

echo "Uninstalling Jaguar Compiler Toolchain from ${PREFIX}..."
rm -f "${PREFIX}/bin/jag"
rm -f "${PREFIX}/bin/jag.exe"
rm -f "${PREFIX}/lib/libjagrt.a"
rm -rf "${PREFIX}/include/jaguar"

echo "Jaguar successfully uninstalled."
