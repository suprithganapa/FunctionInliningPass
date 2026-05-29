#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build"

mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

cmake ..
cmake --build . -j"$(nproc)"

echo "Build complete: ${BUILD_DIR}/libFunctionInliningPass.so"
