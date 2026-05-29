#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TEST_DIR="${PROJECT_ROOT}/tests"
OUT_DIR="${PROJECT_ROOT}/ir"

mkdir -p "${OUT_DIR}"

for file in "${TEST_DIR}"/*.c; do
  base="$(basename "${file}" .c)"
  clang -O0 -S -emit-llvm "${file}" -o "${OUT_DIR}/${base}.ll"
  echo "Generated IR: ${OUT_DIR}/${base}.ll"
done
