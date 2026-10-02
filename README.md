# Caesar CZR

A mobile-first Layer-1 cryptocurrency with a native mining pool, web wallet, block explorer, and a 4-level invite tree — all built to run on a single Android device.

## Status (2026-10-02)

Production-ready local network:
- **Chain height**: 42 blocks
- **3-node sync**: works end-to-end
- **Auto-payout**: verified
- **Invite tree**: 4 levels, persistent
- **PWA**: installable on Android
- **24/7 automation**: watchdog + Termux:Boot

## Features

### Network
- **Node** — full P2P node with mempool, chain replacement, UTXO tracking
- **3-node sync** — verified full-chain replication between peers
- **Auto-reconnect** — dropped connections recovered automatically
- **Heartbeat** — periodic re-sync every 20 seconds

### Wallet
- **Web wallet** — PIN-encrypted, Ed25519 signatures
- **PWA installable** — works offline, appears as native app
- **QR receive** — scan to receive CZR
- **Send CZR** — with confirmation
- **Sign / Verify** — messages signed with private key
- **Address Book** — save frequently-used addresses

### Mining Pool
- **Native pool** — HTTP worker protocol
- **Worker client** — standalone `caesar_worker` binary
- **Share accounting** — persistent across restarts
- **Manual payout** — trigger from UI or API
- **Auto-payout** — every N blocks
- **2% fee** — configurable

### Community (unique)
- **Welcome modal** — "You are Founder #N"
- **Founder badge** — first 1000 users
- **4-level invite tree** — see who invited whom
- **Circle card** — share your invite link
- **Network counter** — live total users

### Dashboard
- **Hero Mining Card** — live hash rate, blocks, progress
- **Top Miners** — leaderboard with medals
- **Network Activity** — bar chart of recent blocks
- **Block Explorer** — full chain browser

## Quick Start (Termux / Linux)

### Build

    cd ~/caesar-czr-internal-real
    cmake -B build-web .
    cmake --build build-web --target caesar_web caesar_worker -j

### Run

    ./build-web/caesar_web \
      --port 18555 \
      --rpc-port 8443 \
      --data ~/.caesar/data-web

Then open: http://127.0.0.1:8443/
PIN: `18555`

### Run a worker

    ./build-web/caesar_worker \
      --pool http://127.0.0.1:8443 \
      --address CZ1... \
      --name my-rig \
      --threads 4

### Run multiple nodes

    # Node A (seed)
    ./build-web/caesar_web --port 18555 --rpc-port 8443 --data ~/.caesar/data-web &

    # Node B (peer)
    ./build-web/caesar_web --port 18556 --rpc-port 8444 \
      --data ~/.caesar/data-web-b \
      --peer 127.0.0.1:18555 &

    # Node C (peer)
    ./build-web/caesar_web --port 18557 --rpc-port 8445 \
      --data ~/.caesar/data-web-c \
      --peer 127.0.0.1:18555 &

## 24/7 Automation

### Watchdog (auto-restart)

    ~/watchdog-caesar.sh

### Boot script (Termux:Boot)

Place in `~/.termux/boot/start-caesar.sh`. Install Termux:Boot from F-Droid.

### Resume after reboot

    ~/resume-caesar.sh

## API

### Chain
| Method | Endpoint | Description |
|--------|----------|-------------|
| GET  | `/api/status` | Chain state |
| GET  | `/api/blocks?limit=N` | Recent blocks |
| GET  | `/api/balance` | Wallet balance (auth) |
| POST | `/api/send` | Send CZR (auth) |
| POST | `/api/mine_default` | Mine one block (auth) |

### Pool
| Method | Endpoint | Description |
|--------|----------|-------------|
| POST | `/api/pool/register` | Register worker |
| GET  | `/api/pool/job` | Get mining job |
| POST | `/api/pool/submit` | Submit share |
| GET  | `/api/pool/stats` | Pool stats |
| POST | `/api/pool/payout` | Trigger payout (auth) |
| GET  | `/api/pool/payouts` | Payout history |
| GET  | `/api/pool/auto/status` | Auto-payout config |
| POST | `/api/pool/auto/config` | Set auto-payout (auth) |
| GET  | `/api/pool/community` | Community stats |

### User / Invite
| Method | Endpoint | Description |
|--------|----------|-------------|
| POST | `/api/user/register` | Register user |
| GET  | `/api/user/me?address=X` | User info |
| GET  | `/api/user/tree?address=X` | 4-level invite tree |
| GET  | `/api/network/stats` | Network growth |

## Storage

All files live in the `--data` directory:

- `blockchain.dat` — the chain
- `wallet.pem` — encrypted Ed25519 key
- `pin.hash` — PIN hash (PBKDF2, 100k)
- `pool_state.txt` — full pool + user state

## Documentation

- [Whitepaper](WHITEPAPER.md)
- [Architecture](docs/ARCHITECTURE.md)
- [Mining](docs/MINING.md)
- [Next Steps](NEXT_STEPS.md)

## License

MIT — see [LICENSE](LICENSE).
