#!/usr/bin/env bash
set -uo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
echo "▶ تطبيق الإصلاحات..."
FIXED=0

# إصلاح NDK variable
[ -f .github/workflows/build-apk.yml ] && sed -i 's/ANDROID_NDK_HOME/ANDROID_NDK_ROOT/g' .github/workflows/build-apk.yml && FIXED=$((FIXED+1))

# إصلاح setup-android packages
if [ -f .github/workflows/build-apk.yml ] && ! grep -q "packages: 'platform-tools" .github/workflows/build-apk.yml; then
    sed -i "s|uses: android-actions/setup-android@v4|uses: android-actions/setup-android@v4\n        with:\n          packages: 'platform-tools platforms;android-34 build-tools;34.0.0'|g" .github/workflows/build-apk.yml
    FIXED=$((FIXED+1))
fi

# إصلاح CMakeLists
cat > android/app/src/main/cpp/CMakeLists.txt << 'CMK'
cmake_minimum_required(VERSION 3.20)
project(caesar_web_android CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../..")
add_compile_definitions(CPPHTTPLIB_NO_IFADDRS=1)
add_executable(caesar_web "${REPO_ROOT}/src/web.cpp")
target_include_directories(caesar_web PRIVATE "${REPO_ROOT}/include" "${REPO_ROOT}/third_party" "${OPENSSL_ROOT}/include")
find_library(CRYPTO_LIB crypto PATHS "${OPENSSL_ROOT}/lib" NO_DEFAULT_PATH)
find_library(CRYPTO_LIB crypto)
if(CRYPTO_LIB)
    target_link_libraries(caesar_web PRIVATE "${CRYPTO_LIB}")
else()
    target_link_libraries(caesar_web PRIVATE "${OPENSSL_ROOT}/lib/libcrypto.a")
endif()
target_link_libraries(caesar_web PRIVATE log)
target_link_options(caesar_web PRIVATE -Wl,--gc-sections -Wl,--exclude-libs,ALL)
CMK
FIXED=$((FIXED+1))

echo "▶ تم تطبيق $FIXED إصلاحات"
