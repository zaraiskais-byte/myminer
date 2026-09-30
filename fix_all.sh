#!/data/data/com.termux/files/usr/bin/bash
set -e

echo "=== [1/6] Removing old workflow ==="
rm -f .github/workflows/build-apk.yml

echo "=== [2/6] Writing robust workflow ==="
mkdir -p .github/workflows
cat > .github/workflows/build-apk.yml << 'YML'
name: Build APK
on:
  workflow_dispatch:
  push:
    branches: [main]
permissions:
  contents: write
jobs:
  build:
    runs-on: ubuntu-22.04
    timeout-minutes: 60
    steps:
      - uses: actions/checkout@v4

      - uses: actions/setup-java@v4
        with:
          distribution: temurin
          java-version: 17

      - name: Setup Android
        run: |
          export ANDROID_HOME=$HOME/android-sdk
          mkdir -p $ANDROID_HOME/cmdline-tools
          cd $ANDROID_HOME/cmdline-tools
          wget -q https://dl.google.com/android/repository/commandlinetools-linux-11076708_latest.zip -O cmdline.zip
          unzip -q cmdline.zip
          mv cmdline-tools latest
          export PATH=$ANDROID_HOME/cmdline-tools/latest/bin:$PATH
          yes | sdkmanager --licenses > /dev/null 2>&1 || true
          sdkmanager "platform-tools" "platforms;android-34" "build-tools;34.0.0" "ndk;25.2.9519653" "cmake;3.22.1"
          echo "ANDROID_HOME=$ANDROID_HOME" >> $GITHUB_ENV
          echo "ANDROID_NDK_HOME=$ANDROID_HOME/ndk/25.2.9519653" >> $GITHUB_ENV
          echo "$ANDROID_HOME/cmdline-tools/latest/bin" >> $GITHUB_PATH

      - name: Get OpenSSL
        run: |
          cd /tmp
          git clone --depth 1 https://github.com/KDAB/android_openssl.git
          echo "OPENSSL_BASE=/tmp/android_openssl/openssl-3" >> $GITHUB_ENV

      - name: Build native binary
        run: |
          cd android/app/src/main/cpp
          mkdir -p build
          cmake -S . -B build \
            -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake \
            -DANDROID_ABI=arm64-v8a \
            -DANDROID_PLATFORM=android-26 \
            -DANDROID_STL=c++_static \
            -DOPENSSL_ROOT=$OPENSSL_BASE/arm64-v8a \
            -DCMAKE_BUILD_TYPE=Release
          cmake --build build --target caesar_web -j4
          cp build/caesar_web ../../assets/caesar_web

      - uses: gradle/actions/setup-gradle@v3
        with:
          gradle-version: 8.5

      - name: Build APK
        run: |
          cd android
          gradle assembleDebug --no-daemon

      - name: Release
        uses: softprops/action-gh-release@v2
        with:
          tag_name: apk-${{ github.run_number }}
          name: "Caesar CZR v${{ github.run_number }}"
          files: android/app/build/outputs/apk/debug/*.apk
YML

echo "=== [3/6] Adding launcher icon ==="
mkdir -p android/app/src/main/res/mipmap-anydpi-v26
cat > android/app/src/main/res/drawable/ic_launcher.xml << 'XML' 2>/dev/null || true
XML
mkdir -p android/app/src/main/res/drawable
cat > android/app/src/main/res/drawable/ic_launcher.xml << 'XML'
<?xml version="1.0" encoding="utf-8"?>
<shape xmlns:android="http://schemas.android.com/apk/res/android" android:shape="rectangle">
    <solid android:color="#f0c040"/>
</shape>
XML

echo "=== [4/6] Fixing OpenSSL path in CMake ==="
cat > android/app/src/main/cpp/CMakeLists.txt << 'CMK'
cmake_minimum_required(VERSION 3.20)
project(caesar_web_android CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../..")
add_executable(caesar_web "${REPO_ROOT}/src/web.cpp")
target_include_directories(caesar_web PRIVATE
    "${REPO_ROOT}/include"
    "${REPO_ROOT}/third_party"
    "${OPENSSL_ROOT}/include")
target_link_libraries(caesar_web PRIVATE
    "${OPENSSL_ROOT}/lib/libcrypto.a"
    log)
target_link_options(caesar_web PRIVATE -Wl,--gc-sections)
CMK

echo "=== [5/6] Cleaning old commits ==="
git add -A
git commit -m "Fix: robust Android APK build (auto-generated)" || true

echo "=== [6/6] Pushing ==="
git push origin main

echo ""
echo "=== DONE ==="
echo "Go to: https://github.com/zaraiskais-byte/myminer/actions"
echo "Wait 15 min. If green -> check Releases"
