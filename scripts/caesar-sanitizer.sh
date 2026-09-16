#!/data/data/com.termux/files/usr/bin/bash
set -euo pipefail

ROOT="$HOME/caesar-czr"
BUILD="$HOME/caesar-sanitizer-build"

echo "========================================"
echo "     CAESAR SANITIZER TEST SYSTEM"
echo "========================================"

echo "[1/4] Preparing sanitizer build..."
rm -rf "$BUILD"
mkdir -p "$BUILD"

echo "[2/4] Configuring with Clang sanitizers..."
cmake -S "$ROOT" -B "$BUILD" \
    -DCAESAR_ENABLE_SANITIZERS=ON

echo "[3/4] Building..."
cmake --build "$BUILD" -j1

echo "[4/4] Running all tests under sanitizers..."
ctest --test-dir "$BUILD" --output-on-failure

echo "========================================"
echo "   ALL SANITIZER TESTS PASSED"
echo "========================================"
