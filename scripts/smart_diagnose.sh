#!/usr/bin/env bash
set -uo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
echo "▶ تشخيص المشروع..."
PROBLEMS=0
grep -q 'setup-android@v3' .github/workflows/*.yml 2>/dev/null && { echo "  ⚠ setup-android v3 قديم"; PROBLEMS=$((PROBLEMS+1)); }
grep -q 'ANDROID_NDK_HOME' .github/workflows/build-apk.yml 2>/dev/null && { echo "  ⚠ ANDROID_NDK_HOME خاطئ"; PROBLEMS=$((PROBLEMS+1)); }
grep -q 'ifaddrs' third_party/httplib.h 2>/dev/null && ! grep -q "CPPHTTPLIB_NO_IFADDRS" android/app/src/main/cpp/CMakeLists.txt 2>/dev/null && { echo "  ⚠ getifaddrs غير معرّف"; PROBLEMS=$((PROBLEMS+1)); }
echo "▶ المشاكل المكتشفة: $PROBLEMS"
