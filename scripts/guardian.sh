#!/usr/bin/env bash
# Caesar CZR Smart Guardian - scans all files, fixes issues
set -uo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

FIXED=0
SCANNED=0
ISSUES=""

log() { echo "[GUARDIAN] $*"; }
fix() { echo "[FIX] $*"; FIXED=$((FIXED+1)); }
issue() { echo "[ISSUE] $*"; ISSUES="$ISSUES\n$*"; }

# 1. Scan all C++ headers/sources
log "Scanning C++ files..."
while IFS= read -r f; do
  SCANNED=$((SCANNED+1))
  # Check missing newline at EOF
  if [ -s "$f" ] && [ "$(tail -c1 "$f" | wc -l)" -eq 0 ]; then
    printf '\n' >> "$f"
    fix "Added newline to $f"
  fi
  # Check trailing whitespace
  if grep -q ' $' "$f" 2>/dev/null; then
    sed -i 's/[[:space:]]*$//' "$f"
    fix "Removed trailing whitespace in $f"
  fi
  # Check tabs in indentation (should be 4 spaces)
  if grep -qP '^\t' "$f" 2>/dev/null; then
    sed -i 's/^\t/    /g' "$f"
    fix "Replaced tabs with spaces in $f"
  fi
done < <(find src include tests fuzz -type f \( -name '*.cpp' -o -name '*.hpp' -o -name '*.h' \) 2>/dev/null)

# 2. Validate all JSON/YAML
log "Validating YAML workflows..."
while IFS= read -r f; do
  SCANNED=$((SCANNED+1))
  if command -v python3 >/dev/null 2>&1; then
    if ! python3 -c "import yaml,sys; yaml.safe_load(open('$f'))" 2>/dev/null; then
      issue "Invalid YAML: $f"
    fi
  fi
done < <(find .github -name '*.yml' -o -name '*.yaml' 2>/dev/null)

# 3. Check Android gradle files
log "Validating Android config..."
for f in android/settings.gradle android/build.gradle android/app/build.gradle; do
  if [ -f "$f" ]; then
    SCANNED=$((SCANNED+1))
    if ! grep -q 'sdk' "$f" 2>/dev/null && [ "$(basename $f)" = "build.gradle" ] && [ "$f" = "android/app/build.gradle" ]; then
      issue "Missing SDK config in $f"
    fi
  fi
done

# 4. Ensure required files exist
log "Checking required files..."
REQUIRED=(
  "CMakeLists.txt"
  "README.md"
  ".gitignore"
  ".clang-format"
  "include/caesar/node.hpp"
  "include/caesar/wallet.hpp"
  "include/caesar/http_rpc.hpp"
  "src/main.cpp"
  "src/web.cpp"
  "third_party/httplib.h"
)
for f in "${REQUIRED[@]}"; do
  SCANNED=$((SCANNED+1))
  if [ ! -f "$f" ]; then
    issue "MISSING FILE: $f"
  fi
done

# 5. Check for hardcoded secrets
log "Scanning for secrets..."
while IFS= read -r f; do
  SCANNED=$((SCANNED+1))
  if grep -qE '(password|secret|api[_-]?key|token)\s*=\s*["\047][^"\047]{8,}' "$f" 2>/dev/null; then
    issue "Possible secret in $f"
  fi
done < <(find src include -type f 2>/dev/null)

# 6. Rebuild check (optional, off by default)
if [ "${GUARDIAN_BUILD:-0}" = "1" ]; then
  log "Testing build..."
  if ! cmake --build build-web --target caesar_web 2>&1 | tail -5; then
    issue "Build failed"
  fi
fi

# Summary
echo ""
echo "═══════════════════════════════════════"
echo "GUARDIAN REPORT"
echo "═══════════════════════════════════════"
echo "Scanned: $SCANNED files"
echo "Fixed:   $FIXED issues"
echo "Issues:  $(echo -e "$ISSUES" | grep -c . || echo 0) remaining"
echo "═══════════════════════════════════════"
if [ -n "$ISSUES" ]; then
  echo -e "Unresolved:$ISSUES"
fi
