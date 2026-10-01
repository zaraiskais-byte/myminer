# Caesar CZR Whitepaper

**Version 0.1 — October 2026**

## Abstract

Caesar CZR (ticker: CZR) is a mobile-first Layer-1 cryptocurrency designed
for participation without specialized hardware. It combines a proof-of-work
consensus with a native mining pool, a web-based wallet, and a full block
explorer — all built to run on commodity Android devices.

This document specifies the protocol, consensus rules, economics, and
the reference implementation.

## 1. Introduction

Most proof-of-work cryptocurrencies require dedicated mining hardware (ASICs)
or expensive GPUs. This excludes the vast majority of the world's population
from participating in network security.

Caesar CZR takes a different approach:

- A CPU-friendly, memory-hard proof-of-work function
- A native pool protocol that runs on the same node software
- A mobile-first wallet interface served over HTTP

The result is a network that can be participated in by anyone with a phone.

## 2. Consensus

### 2.1 Proof of Work

Caesar CZR uses a custom memory-hard proof-of-work function. The hash
operates on the block header and takes as input:

- Version (uint32)
- Height (uint64)
- Previous block hash (32 bytes)
- Merkle root (32 bytes)
- Witness root (32 bytes)
- Timestamp (uint64)
- Nonce (uint64)
- Difficulty (uint32)

The nonce field is zeroed before hashing. The resulting hash must have at
least `difficulty` leading zero bits.

### 2.2 Difficulty Adjustment

Difficulty is adjusted every N blocks using the median block time of the
last window. A floor of 8 leading zero bits is enforced to prevent the
difficulty from dropping to zero on idle networks.

### 2.3 Block Time Target

The target block interval is approximately 60 seconds.

## 3. Block Structure

A block consists of:

- A header (fixed size, serialized binary)
- A list of transactions (variable size)

The coinbase transaction is always the first transaction in a block.
It pays `block_subsidy(height)` to the miner's recipient address.

## 4. Transactions

Each transaction has:

- Version (uint32)
- Coinbase data (variable, only for coinbase)
- Inputs (list)
- Outputs (list)
- Witness set (signatures + public keys)

Each output specifies an amount (in atomic units) and a recipient address.

### 4.1 Addresses

Addresses are derived from an Ed25519 public key, hashed with SHA-256,
and encoded with a CZ1 prefix. Each address is 52 characters long.

### 4.2 Units

- 1 CZR = 100,000,000 atomic units

### 4.3 Signatures

Transaction inputs are signed with Ed25519 using a domain-separated
serialization: `CAESAR_TX_INPUT_SIGNATURE_V1`.

## 5. Economics

### 5.1 Supply

- Maximum supply: 21,000,000 CZR
- Initial block subsidy: 50 CZR
- Halving interval: 210,000 blocks

### 5.2 Mining Pool

The reference implementation includes a native mining pool:

- Pool operators run `caesar_web` with pool endpoints enabled
- Workers run `caesar_worker` and connect over HTTP
- Shares are counted and stored persistently
- The pool takes a configurable fee (default: 2%)
- Payouts are issued automatically every N blocks

### 5.3 Fees

Transaction fees are optional in this version. A future revision will
introduce a fee market.

## 6. Network

### 6.1 Peer-to-Peer

Nodes connect over TCP. Default ports:

- Mainnet: 18444
- Testnet: 18445

### 6.2 Genesis

Each network has a pinned genesis block hash. Nodes refuse to start if
the local chain does not match the expected genesis for the configured
network id.

- Mainnet id: 1
- Testnet id: 2

## 7. Reference Implementation

The reference implementation is written in C++17 and depends only on
OpenSSL and a header-only HTTP library. It builds on Termux, Linux, and
Android.

Components:

- `caesard` — full node + P2P
- `caesar_web` — node + wallet + pool + dashboard
- `caesar_worker` — standalone mining worker

## 8. Roadmap

- [x] Genesis and consensus rules
- [x] P2P handshake and block propagation
- [x] Web wallet with PIN encryption
- [x] Block explorer
- [x] Native mining pool
- [x] Automatic payouts
- [x] Persistent state
- [ ] Multi-node synchronization
- [ ] Mobile worker application
- [ ] Public seed nodes
- [ ] Test suite
- [ ] Docker packaging

## 9. License

MIT License. See LICENSE file.

## 10. Contact

Repository: https://github.com/zaraiskais-byte/myminer
