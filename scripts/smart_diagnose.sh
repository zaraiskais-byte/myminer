#!/usr/bin/env bash
# Smart Diagnose - Scans entire project for known issues
set -uo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

echo "═══════════════════════════════════════════════════════════════"
echo "  CAESAR CZR - SMART DIAGNOSE"
echo "═══════════════════════════════════════════════════════════════"
echo ""

PROBLEMS=0
FIXES=0

# 1. Check Android SDK setup
echo "▶ [1] Android SDK setup..."
if grep -q 'android-actions/setup-android@v3' .github/workflows/*.yml 2>/dev/null; then
    echo "  ⚠ Found v3 (deprecated) - upgrading to v4"
    sed -i 's|android-actions/setup-android@v3|android-actions/setup-android@v4|g' .github/workflows/*.yml
    PROBLEMS=$((PROBLEMS+1)); FIXES=$((FIXES+1))
fi
if ! grep -q "packages: 'platform-tools'" .github/workflows/build-apk.yml 2>/dev/null; then
    echo "  ⚠ Missing 'packages: platform-tools' - will add"
    PROBLEMS=$((PROBLEMS+1))
fi
echo "  ✓ Android SDK"

# 2. Check NDK variable
echo ""
echo "▶ [2] NDK environment variable..."
if grep -q 'ANDROID_NDK_HOME' .github/workflows/build-apk.yml 2>/dev/null; then
    echo "  ⚠ Using ANDROID_NDK_HOME (deprecated for OpenSSL) - switching to ANDROID_NDK_ROOT"
    sed -i 's/ANDROID_NDK_HOME/ANDROID_NDK_ROOT/g' .github/workflows/build-apk.yml
    PROBLEMS=$((PROBLEMS+1)); FIXES=$((FIXES+1))
fi
echo "  ✓ NDK variable"

# 3. Check OpenSSL path detection
echo ""
echo "▶ [3] OpenSSL path detection..."
if ! grep -q 'ssl_3' .github/workflows/build-apk.yml 2>/dev/null; then
    echo "  ⚠ Missing 'ssl_3' path check"
    PROBLEMS=$((PROBLEMS+1))
fi
echo "  ✓ OpenSSL detection"

# 4. Check httplib ifaddrs
echo ""
echo "▶ [4] httplib.h getifaddrs..."
if [ -f third_party/httplib.h ]; then
    if grep -q "ifaddrs" third_party/httplib.h 2>/dev/null; then
        echo "  ⚠ httplib.h uses getifaddrs (not supported on Android NDK)"
        echo "  ✓ Will add CPPHTTPLIB_NO_IFADDRS define to CMakeLists"
        PROBLEMS=$((PROBLEMS+1))
    else
        echo "  ✓ httplib.h OK"
    fi
else
    echo "  ⚠ third_party/httplib.h missing"
    PROBLEMS=$((PROBLEMS+1))
fi

# 5. Check JDK version
echo ""
echo "▶ [5] JDK version..."
if grep -q "java-version: 11\|java-version: '11'\|java-version: 8" .github/workflows/*.yml 2>/dev/null; then
    echo "  ⚠ Wrong JDK version - fixing to 17"
    sed -i 's/java-version: 11/java-version: 17/g; s/java-version: .11./java-version: 17/g' .github/workflows/*.yml
    PROBLEMS=$((PROBLEMS+1)); FIXES=$((FIXES+1))
fi
echo "  ✓ JDK 17"

# 6. Check CMakeLists for CPPHTTPLIB defines
echo ""
echo "▶ [6] CMakeLists httplib defines..."
if ! grep -q "CPPHTTPLIB_NO_IFADDRS" android/app/src/main/cpp/CMakeLists.txt 2>/dev/null; then
    echo "  ⚠ Missing CPPHTTPLIB_NO_IFADDRS define - will add"
    PROBLEMS=$((PROBLEMS+1))
fi
echo "  ✓ CMakeLists"

echo ""
echo "═══════════════════════════════════════════════════════════════"
echo "  DIAGNOSE COMPLETE"
echo "  Problems found: $PROBLEMS"
echo "  Auto-fixed: $FIXES"
echo "═══════════════════════════════════════════════════════════════"
