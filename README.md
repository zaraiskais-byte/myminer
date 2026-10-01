# Caesar CZR

A mobile-first Layer-1 cryptocurrency with a native mining pool, written in C++17.

## Features

- **Node** — full P2P node with mempool, chain replacement, and UTXO tracking
- **Wallet** — web-based, PIN-encrypted, Ed25519 signatures
- **Explorer** — block explorer served from the same process
- **Mining Pool** — native pool with worker protocol, share accounting, and automatic payouts
- **Worker** — standalone binary for external miners

## Quick Start (Termux / Linux)

### Build

    cd ~/caesar-czr-internal-real
    cmake -B build-web .
    cmake --build build-web --target caesar_web caesar_worker -j

### Run Node + Wallet + Pool

    ./build-web/caesar_web \
      --port 18555 \
      --rpc-port 8443 \
      --data ~/.caesar/data-web

Then open:

- http://127.0.0.1:8443/          — wallet
- http://127.0.0.1:8443/explorer  — block explorer
- http://127.0.0.1:8443/pool      — pool dashboard

PIN: `18555` (default from `start-caesar-web.sh`)

### Run a Worker

    ./build-web/caesar_worker \
      --pool http://127.0.0.1:8443 \
      --address CZ1... \
      --name my-worker \
      --threads 4

## API

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET  | `/api/status` | Chain state (height, peers, mempool) |
| GET  | `/api/blocks?limit=N` | Recent blocks |
| POST | `/api/mine_default` | Mine one block (auth) |
| GET  | `/api/balance` | Wallet balance (auth) |
| POST | `/api/send` | Send CZR (auth) |
| POST | `/api/pool/register` | Register a worker |
| GET  | `/api/pool/job` | Fetch current mining job |
| POST | `/api/pool/submit` | Submit share or solution |
| GET  | `/api/pool/stats` | Pool statistics |
| POST | `/api/pool/payout` | Trigger payout (auth) |
| GET  | `/api/pool/payouts` | Payout history |
| GET  | `/api/pool/auto/status` | Auto-payout config |
| POST | `/api/pool/auto/config` | Set auto-payout (auth) |

## Storage

- `~/.caesar/data-web/blockchain.dat` — the chain
- `~/.caesar/data-web/wallet.pem` — encrypted wallet
- `~/.caesar/data-web/pin.hash` — PIN hash (PBKDF2-SHA256, 100k iterations)
- `~/.caesar/data-web/pool_state.txt` — persisted pool state

## Documentation

- [Whitepaper](WHITEPAPER.md)
- [Architecture](docs/ARCHITECTURE.md)
- [Mining](docs/MINING.md)

## License

MIT — see [LICENSE](LICENSE).
