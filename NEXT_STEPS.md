# Caesar CZR - Next Steps

## Current state (2026-10-02, stable network)
- Chain: 38 blocks
- Pool: works, auto-payout works, 2 payouts on-chain
- Wallet: fast (~30ms per API call)
- Community stats: live
- P2P: two nodes sync AND stay connected
- Stable over 60s test

## What works
- Two-node sync: full chain replication
- Gossip: new blocks relay to peers
- Auto-reconnect: drops recovered
- Periodic re-sync: every 20s

## Test it
    cd ~/caesar-czr-internal-real
    ./build-web/caesar_web --port 18555 --rpc-port 8443 --data ~/.caesar/data-web &
    ./build-web/caesar_web --port 18556 --rpc-port 8444 --data ~/.caesar/data-web-b --peer 127.0.0.1:18555 &
    sleep 25
    curl http://127.0.0.1:8443/api/status   # peers: 1
    curl http://127.0.0.1:8444/api/status   # peers: 1, height matches

## Cleanup for production
1. Remove [TRACE] logging from p2p_relay.hpp
2. Reduce verbosity of sync/reader messages
3. Add real heartbeat (only if connections prove unstable over long time)

## Future features
- Peer discovery (DNS seed)
- Persistent peer list on disk
- Multi-peer gossip (3+ nodes)
- VPS seed node deployment
- Android APK for worker

## Priority
READY — network is stable. Next: deploy seed node or add features.
