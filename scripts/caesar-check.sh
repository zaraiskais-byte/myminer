#!/data/data/com.termux/files/usr/bin/bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$ROOT/build"

echo "========================================"
echo "       CAESAR CZR AUTOMATED CHECK"
echo "========================================"
echo "[1/5] Project root..."
echo "$ROOT"

echo "[2/5] Configuring CMake..."
cmake -S "$ROOT" -B "$BUILD"

echo "[3/5] Building Caesar CZR..."
cmake --build "$BUILD" -j1

echo "[4/5] Running node..."
"$BUILD/caesard"

echo "[5/5] Git status..."
cd "$ROOT"
git status --short

echo "========================================"
echo "       CAESAR CZR CHECK PASSED"
echo "========================================"
