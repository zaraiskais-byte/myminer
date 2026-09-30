#!/usr/bin/env bash
# Smart Fix - Repairs all known build issues automatically
set -uo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

echo "═══════════════════════════════════════════════════════════════"
echo "  CAESAR CZR - SMART FIX"
echo "═══════════════════════════════════════════════════════════════"
echo ""

FIXED=0

# 1. Fix AndroidManifest if missing
echo "▶ [1] Ensuring AndroidManifest.xml has icon..."
mkdir -p android/app/src/main/res/mipmap-anydpi-v26
mkdir -p android/app/src/main/res/drawable
mkdir -p android/app/src/main/res/values

if [ ! -f android/app/src/main/res/mipmap-anydpi-v26/ic_launcher.xml ]; then
    cat > android/app/src/main/res/mipmap-anydpi-v26/ic_launcher.xml << 'XML'
<?xml version="1.0" encoding="utf-8"?>
<adaptive-icon xmlns:android="http://schemas.android.com/apk/res/android">
    <background android:drawable="@drawable/ic_bg"/>
    <foreground android:drawable="@drawable/ic_fg"/>
</adaptive-icon>
XML
    FIXED=$((FIXED+1))
fi

# 2. Fix httplib define in CMakeLists
echo ""
echo "▶ [2] Adding CPPHTTPLIB_NO_IFADDRS to CMakeLists..."
if ! grep -q "CPPHTTPLIB_NO_IFADDRS" android/app/src/main/cpp/CMakeLists.txt 2>/dev/null; then
    cat > android/app/src/main/cpp/CMakeLists.txt << 'CMK'
cmake_minimum_required(VERSION 3.20)
project(caesar_web_android CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../..")
message(STATUS "REPO_ROOT: ${REPO_ROOT}")
message(STATUS "OPENSSL_ROOT: ${OPENSSL_ROOT}")

# Define to disable getifaddrs on Android (not supported in NDK)
add_compile_definitions(CPPHTTPLIB_NO_IFADDRS=1)

add_executable(caesar_web "${REPO_ROOT}/src/web.cpp")

target_include_directories(caesar_web PRIVATE
    "${REPO_ROOT}/include"
    "${REPO_ROOT}/third_party"
    "${OPENSSL_ROOT}/include"
)

find_library(CRYPTO_LIB crypto
    PATHS "${OPENSSL_ROOT}/lib"
    NO_DEFAULT_PATH)
find_library(CRYPTO_LIB crypto)

if(CRYPTO_LIB)
    message(STATUS "Using crypto lib: ${CRYPTO_LIB}")
    target_link_libraries(caesar_web PRIVATE "${CRYPTO_LIB}")
else()
    target_link_libraries(caesar_web PRIVATE "${OPENSSL_ROOT}/lib/libcrypto.a")
endif()

target_link_libraries(caesar_web PRIVATE log)
target_link_options(caesar_web PRIVATE -Wl,--gc-sections -Wl,--exclude-libs,ALL)
CMK
    FIXED=$((FIXED+1))
fi
echo "  ✓ CMakeLists fixed"

# 3. Fix NDK variable in workflow
echo ""
echo "▶ [3] Fixing NDK variables..."
if [ -f .github/workflows/build-apk.yml ]; then
    sed -i 's/ANDROID_NDK_HOME/ANDROID_NDK_ROOT/g' .github/workflows/build-apk.yml
    FIXED=$((FIXED+1))
fi
echo "  ✓ NDK variables fixed"

# 4. Fix OpenSSL path detection
echo ""
echo "▶ [4] Fixing OpenSSL path detection..."
if [ -f .github/workflows/build-apk.yml ]; then
    # Replace the entire OpenSSL detection block with robust version
    python3 << 'PYEOF'
import re
path = '.github/workflows/build-apk.yml'
with open(path) as f:
    content = f.read()

# Find and replace OpenSSL detection
new_detection = '''      - name: Detect OpenSSL
        run: |
          if [ -d /tmp/android_openssl/ssl_3 ]; then
            echo "OPENSSL_BASE=/tmp/android_openssl/ssl_3" >> $GITHUB_ENV
            echo "Found: ssl_3"
          elif [ -d /tmp/android_openssl/openssl-3 ]; then
            echo "OPENSSL_BASE=/tmp/android_openssl/openssl-3" >> $GITHUB_ENV
            echo "Found: openssl-3"
          elif [ -d /tmp/android_openssl/openssl-1.1.1 ]; then
            echo "OPENSSL_BASE=/tmp/android_openssl/openssl-1.1.1" >> $GITHUB_ENV
            echo "Found: openssl-1.1.1"
          else
            DIR=$(ls -d /tmp/android_openssl/*/ 2>/dev/null | head -1)
            echo "OPENSSL_BASE=${DIR%/}" >> $GITHUB_ENV
            echo "Found fallback: ${DIR%/}"
          fi
          ls "$OPENSSL_BASE/arm64-v8a" 2>/dev/null | head -5'''

# Replace existing block
pattern = r'      - name: Detect OpenSSL\n        run: \|\n(?:(?!      - name:).)*'
content = re.sub(pattern, new_detection + '\n', content, flags=re.DOTALL)

with open(path, 'w') as f:
    f.write(content)
print("  ✓ OpenSSL detection block replaced")
PYEOF
    FIXED=$((FIXED+1))
fi

# 5. Fix setup-android packages
echo ""
echo "▶ [5] Ensuring setup-android packages..."
if [ -f .github/workflows/build-apk.yml ]; then
    if ! grep -q "packages: 'platform-tools" .github/workflows/build-apk.yml; then
        sed -i "s|uses: android-actions/setup-android@v4|uses: android-actions/setup-android@v4\n        with:\n          packages: 'platform-tools platforms;android-34 build-tools;34.0.0'|g" .github/workflows/build-apk.yml
        FIXED=$((FIXED+1))
    fi
fi
echo "  ✓ setup-android packages"

echo ""
echo "═══════════════════════════════════════════════════════════════"
echo "  SMART FIX COMPLETE - $FIXED fixes applied"
echo "═══════════════════════════════════════════════════════════════"
