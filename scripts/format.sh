#!/usr/bin/env bash
# Caesar CZR — Local code formatter
# Usage: ./scripts/format.sh [check|fix]
set -euo pipefail

MODE="${1:-fix}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

if ! command -v clang-format >/dev/null 2>&1; then
    echo "ERROR: clang-format not installed"
    exit 1
fi

FILES=$(find src include tests fuzz \
    -type f \( -name '*.cpp' -o -name '*.hpp' -o -name '*.h' -o -name '*.cc' \) 2>/dev/null)

if [ -z "$FILES" ]; then
    echo "No C++ files found"
    exit 0
fi

case "$MODE" in
    check)
        FAIL=0
        for f in $FILES; do
            if ! clang-format --dry-run --Werror "$f" >/dev/null 2>&1; then
                echo "NEEDS FORMAT: $f"
                FAIL=1
            fi
        done
        if [ "$FAIL" -eq 1 ]; then
            echo ""
            echo "Run './scripts/format.sh fix' to fix formatting."
            exit 1
        fi
        echo "All files properly formatted."
        ;;
    fix)
        for f in $FILES; do
            clang-format -i "$f"
        done
        echo "Formatted all C++ files."
        ;;
    *)
        echo "Usage: $0 [check|fix]"
        exit 1
        ;;
esac
