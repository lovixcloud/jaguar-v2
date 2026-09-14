#!/usr/bin/env bash
set -e

DEFAULT_PREFIX="/usr/local"
if [ -n "${PREFIX}" ]; then
    TARGET_PREFIX="${PREFIX}"
else
    if [ -d "${HOME}/.local/include/jaguar" ] || [ -f "${HOME}/.local/bin/jag" ] || [ -f "${HOME}/.local/bin/jag.exe" ]; then
        TARGET_PREFIX="${HOME}/.local"
    else
        TARGET_PREFIX="/usr/local"
    fi
fi

echo "Uninstalling Jaguar Compiler Toolchain from ${TARGET_PREFIX}..."
rm -f "${TARGET_PREFIX}/bin/jag"
rm -f "${TARGET_PREFIX}/bin/jag.exe"
rm -f "${TARGET_PREFIX}/lib/libjagrt.a"
rm -rf "${TARGET_PREFIX}/include/jaguar"

echo "Jaguar successfully uninstalled."
