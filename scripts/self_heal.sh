#!/usr/bin/env bash
set -uo pipefail

echo "=== SELF-HEAL: Analyzing failure ==="
FAILED_STEP="${1:-unknown}"
echo "Failed step: $FAILED_STEP"

case "$FAILED_STEP" in
  "Compile native"|"Build native binary"|"Cross-compile caesar_web"*)
    echo "→ Fixing OpenSSL path..."
    if [ -d /tmp/android_openssl/ssl_3 ]; then
      echo "OPENSSL_BASE=/tmp/android_openssl/ssl_3" >> "$GITHUB_ENV"
    elif [ -d /tmp/android_openssl/openssl-3 ]; then
      echo "OPENSSL_BASE=/tmp/android_openssl/openssl-3" >> "$GITHUB_ENV"
    fi
    ;;
  "Build APK"|"Build Debug APK")
    echo "→ Ensuring icons exist..."
    mkdir -p android/app/src/main/res/drawable
    mkdir -p android/app/src/main/res/mipmap-anydpi-v26
    if [ ! -f android/app/src/main/res/drawable/ic_bg.xml ]; then
      printf '%s\n' '<?xml version="1.0" encoding="utf-8"?>' \
        '<shape xmlns:android="http://schemas.android.com/apk/res/android" android:shape="rectangle">' \
        '    <solid android:color="#f0c040"/>' \
        '</shape>' > android/app/src/main/res/drawable/ic_bg.xml
    fi
    ;;
  "Get OpenSSL")
    echo "→ Retrying OpenSSL download..."
    rm -rf /tmp/android_openssl
    git clone --depth 1 https://github.com/KDAB/android_openssl.git /tmp/android_openssl || true
    ;;
  *)
    echo "→ Generic fix: cleaning build dirs..."
    rm -rf android/app/src/main/cpp/build* || true
    rm -rf android/app/build || true
    ;;
esac

echo "=== SELF-HEAL: Done ==="
