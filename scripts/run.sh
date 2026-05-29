#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build"
IR_DIR="${PROJECT_ROOT}/ir"
OPT_DIR="${PROJECT_ROOT}/optimized"
PLUGIN="${BUILD_DIR}/libFunctionInliningPass.so"
THRESHOLD="${1:-10}"

mkdir -p "${OPT_DIR}"

if [[ ! -f "${PLUGIN}" ]]; then
  echo "Plugin not found at ${PLUGIN}. Run scripts/build.sh first."
  exit 1
fi

for input in "${IR_DIR}"/*.ll; do
  base="$(basename "${input}" .ll)"
  output="${OPT_DIR}/${base}.optimized.ll"
  log="${OPT_DIR}/${base}.log"

  echo "Running pass for ${base} (threshold=${THRESHOLD})"
  opt -load-pass-plugin "${PLUGIN}" \
      -passes="simple-inline-pass" \
      -simple-inline-threshold="${THRESHOLD}" \
      "${input}" -S -o "${output}" 2>"${log}"

  echo "Wrote optimized IR: ${output}"
  echo "Wrote pass log:     ${log}"
done
