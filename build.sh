#!/bin/bash
# build.sh — Builds the InlinePass LLVM plugin
set -e

BUILD_DIR="build"
SRC_DIR="src"

echo "========================================="
echo "  Building InlinePass LLVM Plugin"
echo "========================================="

# Check for required tools
if ! command -v clang &> /dev/null; then
    echo "[ERROR] clang not found. Please install LLVM/Clang (e.g. apt install llvm clang)."
    exit 1
fi
if ! command -v opt &> /dev/null; then
    echo "[ERROR] opt not found. Please install LLVM tools (e.g. apt install llvm)."
    exit 1
fi
if ! command -v cmake &> /dev/null; then
    echo "[ERROR] cmake not found. Please install cmake (e.g. apt install cmake)."
    exit 1
fi

echo "[1/3] Preparing build directory..."
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

echo "[2/3] Running CMake configuration..."
cmake "../$SRC_DIR" -DCMAKE_BUILD_TYPE=Release 2>&1 | tail -5

echo "[3/3] Compiling InlinePass..."
make -j"$(nproc)" 2>&1

cd ..

if [ -f "$BUILD_DIR/libInlinePass.so" ]; then
    echo ""
    echo "[OK] Build successful: $BUILD_DIR/libInlinePass.so"
    echo "     Run ./run.sh testcases/test01_basic_leaf.c to try it out."
else
    echo "[ERROR] Build failed — libInlinePass.so not found."
    exit 1
fi
