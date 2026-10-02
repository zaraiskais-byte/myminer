# Caesar CZR — Next Steps

## Current state (2026-10-02)
- Chain: 32 blocks
- Pool: works, auto-payout works, 2 payouts on-chain
- Wallet: fast (~30ms per API call)
- Community stats: live
- P2P: two nodes handshake (peers=1)
- P2P block sync: NOT COMPLETE

## What works
- Node A and Node B connect
- Handshake succeeds
- `--peer host:port` flag added
- `send_get_headers` + `request_sync_from_peer` added

## What's missing
1. **Per-peer reader thread** — Node A does not read frames from
   inbound connections B. Need a thread per peer that:
   - reads P2PFrame from connection
   - calls relay_.handle_frame(peer_id, frame)
2. **handle_get_headers** — must respond with Headers frame
   (send block hashes from requested locator)
3. **handle_headers on Node B** — must request missing blocks
   via GetBlocks
4. **handle_blocks on Node B** — must validate and append

## Where to look
- include/caesar/p2p_relay.hpp: handle_frame, handle_get_headers,
  handle_headers, handle_blocks
- include/caesar/p2p_peer_manager.hpp: connection storage + thread spawn
- include/caesar/p2p_server.hpp: connect_to_peer, accept loop

## Approach for next session
1. Add a reader thread to P2PPeerManager when add_peer is called
2. Reader thread reads frames in a loop and calls a relay callback
3. Wire relay callback in CaesarNode constructor
4. Test: B should sync from A within 5 seconds

## Priority
HIGH — this is the last step toward a real network
