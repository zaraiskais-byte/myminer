#!/data/data/com.termux/files/usr/bin/bash
set -euo pipefail

ROOT="$HOME/caesar-czr"
BUILD="$HOME/caesar-build"

echo "========================================"
echo "       CAESAR CZR CI BUILD SYSTEM"
echo "========================================"

echo "[1/5] Preparing build..."
rm -rf "$BUILD"
mkdir -p "$BUILD"

echo "[2/5] Configuring..."
cmake -S "$ROOT" -B "$BUILD"

echo "[3/5] Building..."
cmake --build "$BUILD" -j1

echo "[4/5] Running automated test suite..."
ctest --test-dir "$BUILD" --output-on-failure

echo "[5/5] Running Caesar node..."
"$BUILD/caesard"

echo "========================================"
echo "       ALL CAESAR CHECKS PASSED"
echo "========================================"
