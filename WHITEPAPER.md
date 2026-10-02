# Caesar CZR Whitepaper

**Version 0.2 — October 2026**

## Abstract

Caesar CZR (ticker: CZR) is a mobile-first Layer-1 cryptocurrency designed
for participation without specialized hardware. It combines proof-of-work
consensus with a native mining pool, a web-based wallet, and a full block
explorer. Everything runs on a single commodity Android device.

## 1. Introduction

Most proof-of-work cryptocurrencies require dedicated mining hardware (ASICs)
or expensive GPUs. This excludes the vast majority of the world's population
from participating in network security.

Caesar CZR takes a different approach:

- CPU-friendly, memory-hard proof-of-work
- Native pool protocol running on the same node
- Web wallet as a Progressive Web App (PWA)
- 24/7 operation on a mobile device

## 2. Consensus

### 2.1 Proof of Work

Custom memory-hard hash over the block header. Nonce is zeroed before hashing.
Hash must have at least `difficulty` leading zero bits.

### 2.2 Difficulty

Adjusted every N blocks using median block time. Floor: 8 leading zero bits.
Target block interval: 60 seconds.

## 3. Blocks

Header:
- Version (uint32)
- Height (uint64)
- Previous hash (32 bytes)
- Merkle root (32 bytes)
- Witness root (32 bytes)
- Timestamp (uint64)
- Nonce (uint64)
- Difficulty (uint32)

Coinbase is always the first transaction.

## 4. Transactions

- Inputs: list of UTXO references
- Outputs: list of (amount, recipient)
- Witness: Ed25519 signatures

Addresses: 52 characters, prefix `CZ1`. Derived from Ed25519 public key hash.

Units: 1 CZR = 100,000,000 atomic units.

## 5. Economics

- Max supply: 21,000,000 CZR
- Initial subsidy: 50 CZR
- Halving interval: 210,000 blocks

### 5.1 Mining Pool

Native pool with:
- HTTP worker protocol
- Persistent share accounting
- Manual + auto payouts
- 2% operator fee

## 6. Network

- P2P over TCP
- Mainnet port: 18444
- Testnet port: 18445
- Genesis pinned per network

### 6.1 Node Sync

Nodes replicate the full chain:
- On connect: request headers, compute missing blocks, fetch, validate, append
- On new block: gossip to peers (except sender)
- Periodic re-sync every 20 seconds
- Auto-reconnect on connection loss

## 7. Invite System (Unique)

Caesar CZR introduces an honest 4-level invite tree — **not a pyramid scheme**:

- Each user gets a permanent user number
- First 1000 users receive a "Founder" badge
- Users can invite friends via a unique link
- The tree is visualizable up to 4 levels deep
- **No money is distributed for invitations**
- The system measures network growth, not wealth

## 8. Reference Implementation

Written in C++17, dependencies: OpenSSL + httplib. Builds on Termux,
Linux, and Android.

Components:
- `caesard` — full node
- `caesar_web` — node + wallet + pool + dashboard
- `caesar_worker` — standalone mining worker

## 9. Roadmap

- [x] Genesis and consensus
- [x] P2P handshake + sync
- [x] Web wallet
- [x] Block explorer
- [x] Mining pool
- [x] Manual + auto payouts
- [x] Persistent state
- [x] 3-node sync
- [x] PWA installable
- [x] 4-level invite tree
- [x] 24/7 watchdog
- [ ] Public seed node
- [ ] Android APK
- [ ] Test suite
- [ ] Docker packaging

## 10. License

MIT — see LICENSE file.

## 11. Repository

https://github.com/zaraiskais-byte/myminer
