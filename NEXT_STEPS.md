# Caesar CZR - Next Steps

## Current state (2026-10-02)
- Chain: 42 blocks
- Pool: working with auto-payout
- Wallet: fast, PWA installed
- Network: 3 nodes sync
- Invite tree: 4 levels, persistent
- Automation: watchdog + Termux:Boot

## Completed (Stage 1 + 2)
- 16 features shipped
- Full documentation
- Persistence across restart
- 24/7 automation

## Immediate next (choose)
### A. Public network reach
- VPS seed node
- DNS seed or hardcoded peer list
- Port forwarding
- Result: peers > 0 from anywhere

### B. Android APK
- Bundle caesar_worker
- Sign with release key
- Distribute directly
- Result: workers on any phone

### C. More features
- Block explorer page redesign
- Send confirmation modal
- Better mining UI
- Result: more polish

### D. Test suite
- Unit tests for pool logic
- Integration tests for sync
- CI/CD with GitHub Actions
- Result: confidence

### E. Advanced
- Light client protocol
- SPV proofs
- Multi-signature wallets
- Result: enterprise-grade

## Recommended order
**A → B → D → C → E**

Reason: Public reach first (network effect), then mobile workers (distribution),
then quality (tests), then polish (UI), then advanced (features).

## How to test current system
    cd ~/caesar-czr-internal-real
    ./build-web/caesar_web --port 18555 --rpc-port 8443 --data ~/.caesar/data-web &
    ./build-web/caesar_web --port 18556 --rpc-port 8444 --data ~/.caesar/data-web-b --peer 127.0.0.1:18555 &
    sleep 15
    curl http://127.0.0.1:8443/api/status
    curl http://127.0.0.1:8444/api/status

## Watchdog + Boot
- Watchdog: `~/watchdog-caesar.sh` (already running)
- Boot: `~/.termux/boot/start-caesar.sh` (ready)
- Requires: Termux:Boot from F-Droid

## Priority
Network reach is highest priority for real adoption.
