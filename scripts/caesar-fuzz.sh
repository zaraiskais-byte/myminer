#!/data/data/com.termux/files/usr/bin/bash
set -euo pipefail

ROOT="$HOME/caesar-czr"
FUZZ_BUILD="$HOME/caesar-fuzz-build"
CORPUS="$ROOT/fuzz/corpus"
ARTIFACTS="$ROOT/fuzz/artifacts"

echo "========================================"
echo "       CAESAR AUTOMATED FUZZING"
echo "========================================"

echo "[1/6] Preparing directories..."
rm -rf "$FUZZ_BUILD"
mkdir -p "$FUZZ_BUILD"
mkdir -p "$CORPUS/transaction"
mkdir -p "$CORPUS/block"
mkdir -p "$CORPUS/crypto"
mkdir -p "$ARTIFACTS"

echo "[2/6] Building Transaction Fuzzer..."
clang++ -std=c++20 \
    -fsanitize=fuzzer,address,undefined \
    -fno-omit-frame-pointer \
    -fno-sanitize-recover=all \
    -I"$ROOT/include" \
    "$ROOT/fuzz/transaction_fuzzer.cpp" \
    -lcrypto \
    -o "$FUZZ_BUILD/transaction-fuzzer"

echo "[3/6] Building Block Fuzzer..."
clang++ -std=c++20 \
    -fsanitize=fuzzer,address,undefined \
    -fno-omit-frame-pointer \
    -fno-sanitize-recover=all \
    -I"$ROOT/include" \
    "$ROOT/fuzz/block_fuzzer.cpp" \
    -lcrypto \
    -o "$FUZZ_BUILD/block-fuzzer"

echo "[4/6] Building Crypto/Merkle Fuzzer..."
clang++ -std=c++20 \
    -fsanitize=fuzzer,address,undefined \
    -fno-omit-frame-pointer \
    -fno-sanitize-recover=all \
    -I"$ROOT/include" \
    "$ROOT/fuzz/crypto_fuzzer.cpp" \
    -lcrypto \
    -o "$FUZZ_BUILD/crypto-fuzzer"

echo "[5/6] Running Transaction Fuzzer..."
"$FUZZ_BUILD/transaction-fuzzer" \
    -runs=10000 \
    -max_len=4096 \
    "$CORPUS/transaction" \
    -artifact_prefix="$ARTIFACTS/transaction-"

echo "[5/6] Running Block Fuzzer..."
"$FUZZ_BUILD/block-fuzzer" \
    -runs=10000 \
    -max_len=16384 \
    "$CORPUS/block" \
    -artifact_prefix="$ARTIFACTS/block-"

echo "[5/6] Running Crypto/Merkle Fuzzer..."
"$FUZZ_BUILD/crypto-fuzzer" \
    -runs=10000 \
    -max_len=4096 \
    "$CORPUS/crypto" \
    -artifact_prefix="$ARTIFACTS/crypto-"

echo "[6/6] Fuzzing completed successfully."
echo "========================================"
echo "       ALL FUZZ TESTS PASSED"
echo "========================================"
