#!/data/data/com.termux/files/usr/bin/bash
echo "=== Monitoring GitHub Actions ==="
echo "Checking every 60 seconds for 20 minutes..."
echo ""

REPO="zaraiskais-byte/myminer"
for i in $(seq 1 20); do
  echo "[$i/20] Checking build status..."
  
  STATUS=$(curl -s "https://api.github.com/repos/$REPO/actions/runs?per_page=1" \
    | grep -o '"status":"[^"]*"' | head -1 | cut -d'"' -f4)
  CONCLUSION=$(curl -s "https://api.github.com/repos/$REPO/actions/runs?per_page=1" \
    | grep -o '"conclusion":"[^"]*"' | head -1 | cut -d'"' -f4)
  
  echo "  Status: $STATUS | Conclusion: $CONCLUSION"
  
  if [ "$STATUS" = "completed" ]; then
    if [ "$CONCLUSION" = "success" ]; then
      echo ""
      echo "=== BUILD SUCCESSFUL ==="
      echo "Downloading APK..."
      
      mkdir -p ~/storage/downloads/CaesarCZR
      
      # Get latest release APK URL
      APK_URL=$(curl -s "https://api.github.com/repos/$REPO/releases/latest" \
        | grep -o '"browser_download_url":"[^"]*\.apk"' \
        | head -1 | cut -d'"' -f4)
      
      if [ -n "$APK_URL" ]; then
        echo "APK URL: $APK_URL"
        curl -L -o ~/storage/downloads/CaesarCZR/CaesarCZR.apk "$APK_URL"
        echo ""
        echo "=== APK SAVED ==="
        echo "Location: ~/storage/downloads/CaesarCZR/CaesarCZR.apk"
        echo "Open Files app → Downloads → CaesarCZR → install"
      else
        echo "No APK found yet. Check releases page manually."
      fi
      exit 0
    else
      echo ""
      echo "=== BUILD FAILED ==="
      echo "Self-heal will run automatically. Waiting for retry..."
    fi
  fi
  
  sleep 60
done

echo ""
echo "=== Timeout: check manually ==="
echo "https://github.com/zaraiskais-byte/myminer/actions"
