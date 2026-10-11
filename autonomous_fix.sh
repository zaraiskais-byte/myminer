#!/usr/bin/env bash
echo "=== CZR AUTONOMOUS CORE CHECK ==="
echo "Checking server status on port 8455..."
if curl -s http://127.0.0.1:8455 > /dev/null; then
    echo "SERVER_STATUS=ONLINE"
else
    echo "SERVER_STATUS=OFFLINE - Restarting server..."
    ./build-web/caesar_web --port 8455 &
fi
echo "Autonomous check completed successfully."
