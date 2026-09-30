#!/data/data/com.termux/files/usr/bin/bash
set -e

echo "=== [1/8] Cleaning old workflow ==="
rm -f .github/workflows/build-apk.yml

echo "=== [2/8] Creating correct launcher icons ==="
mkdir -p android/app/src/main/res/mipmap-anydpi-v26
mkdir -p android/app/src/main/res/mipmap-hdpi
mkdir -p android/app/src/main/res/values
mkdir -p android/app/src/main/res/drawable

cat > android/app/src/main/res/drawable/ic_launcher_background.xml << 'XML'
<?xml version="1.0" encoding="utf-8"?>
<shape xmlns:android="http://schemas.android.com/apk/res/android" android:shape="rectangle">
    <solid android:color="#f0c040"/>
</shape>
XML

cat > android/app/src/main/res/drawable/ic_launcher_foreground.xml << 'XML'
<?xml version="1.0" encoding="utf-8"?>
<vector xmlns:android="http://schemas.android.com/apk/res/android"
    android:width="108dp" android:height="108dp"
    android:viewportWidth="108" android:viewportHeight="108">
    <path android:fillColor="#0f1115"
        android:pathData="M54,30 L54,78 M30,54 L78,54"
        android:strokeWidth="8" android:strokeColor="#0f1115"/>
</vector>
XML

cat > android/app/src/main/res/mipmap-anydpi-v26/ic_launcher.xml << 'XML'
<?xml version="1.0" encoding="utf-8"?>
<adaptive-icon xmlns:android="http://schemas.android.com/apk/res/android">
    <background android:drawable="@drawable/ic_launcher_background"/>
    <foreground android:drawable="@drawable/ic_launcher_foreground"/>
</adaptive-icon>
XML

cat > android/app/src/main/res/mipmap-anydpi-v26/ic_launcher_round.xml << 'XML'
<?xml version="1.0" encoding="utf-8"?>
<adaptive-icon xmlns:android="http://schemas.android.com/apk/res/android">
    <background android:drawable="@drawable/ic_launcher_background"/>
    <foreground android:drawable="@drawable/ic_launcher_foreground"/>
</adaptive-icon>
XML

cat > android/app/src/main/res/values/colors.xml << 'XML'
<?xml version="1.0" encoding="utf-8"?>
<resources>
    <color name="ic_launcher_background">#f0c040</color>
</resources>
XML

echo "=== [3/8] Adding icon reference to AndroidManifest ==="
cat > android/app/src/main/AndroidManifest.xml << 'XML'
<?xml version="1.0" encoding="utf-8"?>
<manifest xmlns:android="http://schemas.android.com/apk/res/android">
<uses-permission android:name="android.permission.INTERNET"/>
<uses-permission android:name="android.permission.ACCESS_NETWORK_STATE"/>
<application
    android:label="Caesar CZR"
    android:icon="@mipmap/ic_launcher"
    android:roundIcon="@mipmap/ic_launcher_round"
    android:usesCleartextTraffic="true"
    android:allowBackup="false"
    android:theme="@android:style/Theme.NoTitleBar">
<activity
    android:name=".MainActivity"
    android:exported="true"
    android:configChanges="orientation|screenSize|keyboardHidden|screenLayout">
<intent-filter>
<action android:name="android.intent.action.MAIN"/>
<category android:name="android.intent.category.LAUNCHER"/>
</intent-filter>
</activity>
</application>
</manifest>
XML

echo "=== [4/8] Fixing CMakeLists (correct OpenSSL path) ==="
cat > android/app/src/main/cpp/CMakeLists.txt << 'CMK'
cmake_minimum_required(VERSION 3.20)
project(caesar_web_android CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../..")

message(STATUS "REPO_ROOT: ${REPO_ROOT}")
message(STATUS "OPENSSL_ROOT: ${OPENSSL_ROOT}")

add_executable(caesar_web "${REPO_ROOT}/src/web.cpp")

target_include_directories(caesar_web PRIVATE
    "${REPO_ROOT}/include"
    "${REPO_ROOT}/third_party"
    "${OPENSSL_ROOT}/include"
)

# Use find_library for correct discovery
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

target_link_options(caesar_web PRIVATE
    -Wl,--gc-sections
    -Wl,--exclude-libs,ALL
)
CMK

echo "=== [5/8] Writing production workflow ==="
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
      - name: Checkout
        uses: actions/checkout@v4

      - name: Setup JDK 17
        uses: actions/setup-java@v4
        with:
          distribution: temurin
          java-version: 17

      - name: Setup Android SDK
        uses: android-actions/setup-android@v4
        with:
          packages: 'platform-tools platforms;android-34 build-tools;34.0.0'

      - name: Install NDK and CMake
        run: |
          yes | sdkmanager --licenses > /dev/null 2>&1 || true
          sdkmanager "ndk;25.2.9519653" "cmake;3.22.1"
          echo "ANDROID_NDK_HOME=${ANDROID_SDK_ROOT}/ndk/25.2.9519653" >> $GITHUB_ENV

      - name: Get prebuilt OpenSSL
        run: |
          cd /tmp
          git clone --depth 1 https://github.com/KDAB/android_openssl.git
          ls /tmp/android_openssl/

      - name: Detect OpenSSL path
        run: |
          DIR=$(ls -d /tmp/android_openssl/ssl_3 2>/dev/null || ls -d /tmp/android_openssl/openssl-* 2>/dev/null | head -1)
          echo "OPENSSL_BASE=$DIR" >> $GITHUB_ENV
          echo "Detected: $DIR"
          ls "$DIR/arm64-v8a" 2>/dev/null || ls "$DIR" | head -10

      - name: Cross-compile caesar_web for ARM64
        run: |
          cd android/app/src/main/cpp
          mkdir -p build-arm64
          cmake -S . -B build-arm64 \
            -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake \
            -DANDROID_ABI=arm64-v8a \
            -DANDROID_PLATFORM=android-26 \
            -DANDROID_STL=c++_static \
            -DOPENSSL_ROOT=$OPENSSL_BASE/arm64-v8a \
            -DCMAKE_BUILD_TYPE=Release
          cmake --build build-arm64 --target caesar_web -j4
          file build-arm64/caesar_web
          cp build-arm64/caesar_web ../../assets/caesar_web
          echo "Binary size: $(stat -c%s ../../assets/caesar_web) bytes"

      - name: Setup Gradle
        uses: gradle/actions/setup-gradle@v3
        with:
          gradle-version: 8.5

      - name: Build Debug APK
        run: |
          cd android
          gradle assembleDebug --no-daemon

      - name: Create Release with APK
        uses: softprops/action-gh-release@v2
        with:
          tag_name: apk-v${{ github.run_number }}
          name: "Caesar CZR Wallet v${{ github.run_number }}"
          body: |
            Caesar CZR Wallet - Auto-built APK
            Download and install on Android.
          files: android/app/build/outputs/apk/debug/*.apk
          draft: false
          prerelease: false
YML

echo "=== [6/8] Adding .gitignore entries ==="
cat >> .gitignore << 'GI'
android/.gradle/
android/build/
android/app/build/
android/local.properties
android/.idea/
android/app/src/main/assets/caesar_web
