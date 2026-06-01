#!/bin/bash
# run.sh — Runs InlinePass on a given C source file (or all testcases)
set -e

BUILD_DIR="build"
PASS_SO="$BUILD_DIR/libInlinePass.so"
RESULTS_DIR="results"

# ── Colour helpers ─────────────────────────────────────────────────────────────
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

print_header() { echo -e "\n${YELLOW}========== $1 ==========${NC}"; }

# ── Pre-flight checks ──────────────────────────────────────────────────────────
if [ ! -f "$PASS_SO" ]; then
    echo -e "${RED}[ERROR] Pass library not found. Run ./build.sh first.${NC}"
    exit 1
fi

mkdir -p "$RESULTS_DIR"

# ── Run a single test ──────────────────────────────────────────────────────────
run_test() {
    local SRC="$1"
    local BASENAME
    BASENAME=$(basename "${SRC%.c}")
    local ORIG_IR="$RESULTS_DIR/${BASENAME}.ll"
    local OPT_IR="$RESULTS_DIR/${BASENAME}_opt.ll"
    local LOG="$RESULTS_DIR/${BASENAME}.log"

    print_header "$BASENAME"

    # Generate unoptimised LLVM IR
    clang -O0 -S -emit-llvm -fno-discard-value-names "$SRC" -o "$ORIG_IR"

    # Count instructions in original IR
    ORIG_INSTR=$(grep -cE '^\s+[^;].*=' "$ORIG_IR" 2>/dev/null || echo 0)

    # Run the pass (capture stderr — that's where pass logs go)
    opt --load-pass-plugin="$PASS_SO" \
        --passes="inline-pass" \
        "$ORIG_IR" -S -o "$OPT_IR" 2>"$LOG"

    # Count instructions in optimised IR
    OPT_INSTR=$(grep -cE '^\s+[^;].*=' "$OPT_IR" 2>/dev/null || echo 0)

    # Print pass log
    cat "$LOG"

    # Diff summary
    echo ""
    echo -e "${GREEN}--- Side-by-Side Comparison (Original vs Optimized) ---${NC}"
    echo "Format: [Original IR]                                | [Optimized IR]"
    diff -y -W 150 --suppress-common-lines "$ORIG_IR" "$OPT_IR" || true
 

}

# ── Entry point ────────────────────────────────────────────────────────────────
if [ "$#" -eq 0 ]; then
    # Run ALL testcases
    echo "No source specified — running all testcases in ./testcases/ ..."
    PASS_COUNT=0
    FAIL_COUNT=0
    for TC in testcases/test*.c; do
        run_test "$TC" && PASS_COUNT=$((PASS_COUNT+1)) || FAIL_COUNT=$((FAIL_COUNT+1))
    done
    echo ""
    echo -e "${GREEN}All tests complete.${NC}  Passed: $PASS_COUNT  Failed: $FAIL_COUNT"
    echo "Results saved to: $RESULTS_DIR/"
elif [ "$1" == "--help" ] || [ "$1" == "-h" ]; then
    echo "Usage:"
    echo "  ./run.sh                          # run all testcases"
    echo "  ./run.sh testcases/test01_basic_leaf.c   # run a single test"
    echo "  ./run.sh path/to/any_file.c       # run on any C source"
else
    run_test "$1"
fi
