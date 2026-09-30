#!/usr/bin/env bash
set -uo pipefail
REPO="zaraiskais-byte/myminer"
STATE_FILE="$HOME/.caesar/last_build_state"
EMAIL_SCRIPT="$HOME/.caesar/send_email.py"
mkdir -p "$(dirname "$STATE_FILE")"
echo "▶ المراقب الذكي يعمل (PID $$)"
while true; do
    RESPONSE=$(curl -s "https://api.github.com/repos/$REPO/actions/runs?per_page=1" 2>/dev/null)
    if [ -n "$RESPONSE" ]; then
        STATUS=$(echo "$RESPONSE" | grep -o '"status":"[^"]*"' | head -1 | cut -d'"' -f4)
        CONCLUSION=$(echo "$RESPONSE" | grep -o '"conclusion":"[^"]*"' | head -1 | cut -d'"' -f4)
        RUN_ID=$(echo "$RESPONSE" | grep -o '"id":[0-9]*' | head -1 | grep -o '[0-9]*')
        CURRENT="$STATUS|$CONCLUSION|$RUN_ID"
        LAST=$(cat "$STATE_FILE" 2>/dev/null || echo "")
        if [ "$CURRENT" != "$LAST" ]; then
            echo "$CURRENT" > "$STATE_FILE"
            RUN_URL="https://github.com/$REPO/actions/runs/$RUN_ID"
            case "$STATUS|$CONCLUSION" in
                "completed|success") python3 "$EMAIL_SCRIPT" "✅ Caesar CZR: Build SUCCESS" "APK: https://github.com/$REPO/releases/latest\nRun: $RUN_URL" 2>/dev/null ;;
                "completed|failure") python3 "$EMAIL_SCRIPT" "❌ Caesar CZR: Build FAILED" "Run: $RUN_URL" 2>/dev/null ;;
            esac
        fi
    fi
    sleep 300
done
