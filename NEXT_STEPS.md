# Caesar CZR - Next Steps

## Current state (2026-10-02, milestone day)
- Chain: 33 blocks
- Pool: works, auto-payout works, 2 payouts on-chain
- Wallet: fast (~30ms per API call)
- Community stats: live
- P2P: two nodes sync full chain
- P2P block sync: WORKS

## What works
- Node A and Node B connect via --peer host:port
- Handshake succeeds
- Full chain sync (Headers -> Blocks -> validate -> append)
- Cache invalidation ensures /api/status reflects new height

## Cleanup needed for production
1. Remove [TRACE] logging from p2p_relay.hpp
2. Add gossip: rebroadcast blocks but ONLY to other peers
3. Reduce log verbosity in production builds

## Future improvements
- Periodic re-sync (every N minutes)
- Peer discovery (DNS seed, hardcoded list)
- Persistent peer list on disk
- Ban list persistence across restarts

## How to test
    cd ~/caesar-czr-internal-real
    ./build-web/caesar_web --port 18555 --rpc-port 8443 --data ~/.caesar/data-web &
    ./build-web/caesar_web --port 18556 --rpc-port 8444 --data ~/.caesar/data-web-b --peer 127.0.0.1:18555 &
    sleep 15
    curl http://127.0.0.1:8443/api/status
    curl http://127.0.0.1:8444/api/status

## Priority
MEDIUM - sync works. Now build real network reach (VPS/seed node).
