#!/data/data/com.termux/files/usr/bin/bash
cd "$HOME/caesar-czr-internal-real" || exit 1
set -uo pipefail

SRC="include/caesar/http_rpc.hpp"
BIN="./build-web/caesar_web"
DATA="$HOME/.caesar/data-web"
LOG="$HOME/caesar-web.log"
RPC="http://127.0.0.1:8443"

pkill -9 -f caesar_web 2>/dev/null || true
sleep 2
rm -f "$BIN"
cmake --build ./build-web --target caesar_web -j2
test -x "$BIN" || exit 1

nohup "$BIN" --port 18555 --rpc-port 8443 --data "$DATA" > "$LOG" 2>&1 &
for i in $(seq 1 40); do
  kill -0 $! 2>/dev/null && curl -fsS --max-time 2 "$RPC/api/status" >/dev/null 2>&1 && break
  sleep 1
done

curl -fsS -H "Cache-Control: no-cache" "$RPC/pool" | grep -c "CZR-XOF-APEX-LIVE-BADGE-HTML"
echo "OPEN: http://127.0.0.1:8443/pool"
