#!/usr/bin/env bash
set -e

DEFAULT_PREFIX="/usr/local"
if [ -n "${PREFIX}" ]; then
    TARGET_PREFIX="${PREFIX}"
else
    # Check if /usr/local is writable without sudo
    if [ -w "/usr/local" ] || [ -w "/usr/local/bin" ] 2>/dev/null; then
        TARGET_PREFIX="/usr/local"
    else
        TARGET_PREFIX="${HOME}/.local"
    fi
fi

echo "Building Jaguar Compiler Toolchain..."
make clean
make

EXE_NAME="jag"
if [ -f "jag.exe" ]; then
    EXE_NAME="jag.exe"
fi

echo "Installing ${EXE_NAME} binary and runtime library to ${TARGET_PREFIX}..."
mkdir -p "${TARGET_PREFIX}/bin" 2>/dev/null || {
    TARGET_PREFIX="${HOME}/.local"
    mkdir -p "${TARGET_PREFIX}/bin"
}
mkdir -p "${TARGET_PREFIX}/lib" 2>/dev/null || true
mkdir -p "${TARGET_PREFIX}/include/jaguar" 2>/dev/null || true

rm -f "${TARGET_PREFIX}/bin/${EXE_NAME}" 2>/dev/null || true
cp "${EXE_NAME}" "${TARGET_PREFIX}/bin/${EXE_NAME}"
chmod 0755 "${TARGET_PREFIX}/bin/${EXE_NAME}" 2>/dev/null || true

if [ -f "libjagrt.a" ]; then
    cp libjagrt.a "${TARGET_PREFIX}/lib/libjagrt.a"
fi

cp -r runtime/* "${TARGET_PREFIX}/include/jaguar/" 2>/dev/null || true

echo "Jaguar successfully installed to ${TARGET_PREFIX}!"
echo "Ensure ${TARGET_PREFIX}/bin is in your PATH. Run '${EXE_NAME} --version' to verify."
