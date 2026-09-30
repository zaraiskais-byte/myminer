#!/data/data/com.termux/files/usr/bin/bash
set -e
R="$HOME/caesar-czr"
cd "$R"

mkdir -p android/app/src/main/java/com/caesarczr/wallet
mkdir -p android/app/src/main/assets
mkdir -p android/app/src/main/res/values
mkdir -p android/app/src/main/cpp
mkdir -p .github/workflows

cat > android/settings.gradle << 'X'
pluginManagement{repositories{google();mavenCentral();gradlePluginPortal()}}
dependencyResolutionManagement{repositoriesMode.set(RepositoriesMode.PREFER_SETTINGS);repositories{google();mavenCentral()}}
rootProject.name="CaesarCZR"
include ':app'
X

cat > android/build.gradle << 'X'
plugins{id 'com.android.application' version '8.2.2' apply false}
X

cat > android/gradle.properties << 'X'
org.gradle.jvmargs=-Xmx2048m -Dfile.encoding=UTF-8
android.useAndroidX=true
android.enableJetifier=false
android.nonTransitiveRClass=true
X

cat > android/app/build.gradle << 'X'
plugins{id 'com.android.application'}
android {
    namespace 'com.caesarczr.wallet'
    compileSdk 34
    defaultConfig {
        applicationId "com.caesarczr.wallet"
        minSdk 26
        targetSdk 34
        versionCode 1
        versionName "1.0"
    }
    buildTypes {
        release { minifyEnabled false }
        debug { debuggable true }
    }
    compileOptions {
        sourceCompatibility JavaVersion.VERSION_17
        targetCompatibility JavaVersion.VERSION_17
    }
    packagingOptions { jniLibs { useLegacyPackaging = true } }
    aaptOptions { noCompress "caesar_web" }
    sourceSets { main { assets.srcDirs = ['src/main/assets'] } }
}
X

cat > android/app/src/main/AndroidManifest.xml << 'X'
<?xml version="1.0" encoding="utf-8"?>
<manifest xmlns:android="http://schemas.android.com/apk/res/android">
<uses-permission android:name="android.permission.INTERNET"/>
<uses-permission android:name="android.permission.ACCESS_NETWORK_STATE"/>
<application android:label="Caesar CZR" android:usesCleartextTraffic="true" android:allowBackup="false" android:theme="@android:style/Theme.NoTitleBar">
<activity android:name=".MainActivity" android:exported="true" android:configChanges="orientation|screenSize|keyboardHidden|screenLayout">
<intent-filter>
<action android:name="android.intent.action.MAIN"/>
<category android:name="android.intent.category.LAUNCHER"/>
</intent-filter>
</activity>
</application>
</manifest>
X

cat > android/app/src/main/res/values/strings.xml << 'X'
<?xml version="1.0" encoding="utf-8"?>
<resources><string name="app_name">Caesar CZR</string></resources>
X

cat > android/app/src/main/java/com/caesarczr/wallet/MainActivity.java << 'X'
package com.caesarczr.wallet;
import android.app.Activity;
import android.os.Bundle;
import android.util.Log;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.Socket;
public class MainActivity extends Activity {
    private static final String TAG = "CaesarCZR";
    private static final int RPC_PORT = 8332;
    private static final int P2P_PORT = 18555;
    private Process nodeProcess;
    private WebView webView;
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        File filesDir = getFilesDir();
        File binary = new File(filesDir, "caesar_web");
        if (!binary.exists()) {
            try { extractAsset("caesar_web", binary); } catch (Exception e) { Log.e(TAG, "extract failed", e); }
        }
        binary.setExecutable(true, false);
        binary.setReadable(true, false);
        File dataDir = new File(filesDir, "data");
        dataDir.mkdirs();
        try {
            ProcessBuilder pb = new ProcessBuilder(binary.getAbsolutePath(), "--data", dataDir.getAbsolutePath(), "--rpc-port", String.valueOf(RPC_PORT), "--port", String.valueOf(P2P_PORT));
            pb.redirectErrorStream(true);
            pb.directory(filesDir);
            nodeProcess = pb.start();
        } catch (Exception e) { Log.e(TAG, "spawn failed", e); }
        waitForServer();
        webView = new WebView(this);
        WebSettings s = webView.getSettings();
        s.setJavaScriptEnabled(true);
        s.setDomStorageEnabled(true);
        s.setLoadWithOverviewMode(true);
        s.setUseWideViewPort(true);
        webView.setWebViewClient(new WebViewClient());
        webView.loadUrl("http://127.0.0.1:" + RPC_PORT);
        setContentView(webView);
    }
    private void extractAsset(String name, File dest) throws Exception {
        try (InputStream in = getAssets().open(name); OutputStream out = new FileOutputStream(dest)) {
            byte[] buf = new byte[8192];
            int n;
            while ((n = in.read(buf)) > 0) out.write(buf, 0, n);
        }
    }
    private void waitForServer() {
        for (int i = 0; i < 60; i++) {
            try (Socket sock = new Socket("127.0.0.1", RPC_PORT)) { return; }
            catch (Exception ignored) { try { Thread.sleep(100); } catch (InterruptedException ignored2) {} }
        }
    }
    @Override
    protected void onDestroy() {
        super.onDestroy();
        if (nodeProcess != null) { nodeProcess.destroy(); nodeProcess = null; }
    }
    @Override
    public void onBackPressed() {
        if (webView != null && webView.canGoBack()) webView.goBack();
        else super.onBackPressed();
    }
}
X

cat > android/app/src/main/cpp/CMakeLists.txt << 'X'
cmake_minimum_required(VERSION 3.20)
project(caesar_web_android CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../..")
add_executable(caesar_web "${REPO_ROOT}/src/web.cpp")
target_include_directories(caesar_web PRIVATE "${REPO_ROOT}/include" "${REPO_ROOT}/third_party" "${OPENSSL_ROOT}/include")
target_link_libraries(caesar_web PRIVATE "${OPENSSL_ROOT}/lib/libcrypto.a" log)
target_link_options(caesar_web PRIVATE -Wl,--gc-sections -Wl,--exclude-libs,ALL)
X

cat > .github/workflows/build-apk.yml << 'X'
name: Build Android APK
on:
  workflow_dispatch:
  push:
    branches: ["main"]
permissions:
  contents: write
jobs:
  build-apk:
    runs-on: ubuntu-latest
    timeout-minutes: 45
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-java@v4
        with:
          distribution: temurin
          java-version: 17
      - uses: android-actions/setup-android@v3
      - name: Install NDK
        run: |
          yes | sdkmanager --licenses > /dev/null 2>&1 || true
          sdkmanager "ndk;25.2.9519653" "cmake;3.22.1"
          echo "ANDROID_NDK_HOME=${ANDROID_HOME}/ndk/25.2.9519653" >> $GITHUB_ENV
      - name: Download OpenSSL
        run: |
          cd /tmp
          git clone --depth 1 https://github.com/KDAB/android_openssl.git
      - name: Detect OpenSSL
        run: |
          DIR=$(ls -d /tmp/android_openssl/openssl-* | head -1)
          echo "OPENSSL_BASE=$DIR" >> $GITHUB_ENV
      - name: Cross-compile caesar_web
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
          cp build-arm64/caesar_web ../../assets/caesar_web
      - uses: gradle/gradle-build-action@v3
        with:
          gradle-version: 8.5
      - name: Build APK
        run: |
          cd android
          gradle assembleDebug --no-daemon --stacktrace
      - name: Release APK
        uses: softprops/action-gh-release@v2
        with:
          tag_name: apk-v${{ github.run_number }}
          name: "Caesar CZR Wallet v${{ github.run_number }}"
          body: "Auto-built APK. Download and install on Android."
          files: android/app/build/outputs/apk/debug/*.apk
          draft: false
          prerelease: false
X

cat >> .gitignore << 'X'

# Android
android/.gradle/
android/build/
android/app/build/
android/local.properties
android/.idea/
android/app/src/main/assets/caesar_web
X

git add -A
git commit -m "Add Android APK auto-build with GitHub Releases"
git push origin main
