# Caesar CZR — Technical Whitepaper

**Version:** 0.1 (Draft)
**Date:** 2026-10-01
**Author:** zaraiskais-byte
**Repository:** https://github.com/zaraiskais-byte/myminer

## 1. Abstract

Caesar CZR (ticker: CZR) is an experimental proof-of-work
cryptocurrency focused on simplicity, verifiable cryptography, and
local-first design. It provides a working blockchain, a
standards-compliant wallet (BIP39 + SLIP-0010 + Ed25519), a P2P
network, and a self-contained web UI — all in portable C++20 with
minimal dependencies.

This document describes the protocol, cryptography, storage model,
network layer, and design goals of CZR. It is intended as a
technical reference for contributors and auditors.

## 2. Design Goals

1. **Verifiable cryptography.** Every cryptographic primitive is
   derived from standards (BIP39, SLIP-0010, Ed25519, AES-256-GCM,
   PBKDF2) and tested against published test vectors.
2. **Deterministic recovery.** A wallet is fully recoverable from
   a 12-word BIP39 mnemonic. No server, no backup file, no external
   service required.
3. **Encrypted at rest.** Wallet keys are stored encrypted with a
   PIN-derived key (AES-256-GCM). Partial writes are prevented via
   atomic rename + fsync.
4. **Local-first.** A full node, wallet, block explorer, and web UI
   run on a single machine, including mobile (Termux/Android).
5. **Minimal dependencies.** Standard C++20, OpenSSL for crypto,
   cpp-httplib for HTTP, qrcodegen for QR.

## 3. Cryptographic Primitives

### 3.1 Key Generation

Wallets are derived from 128 bits of entropy via BIP39:

    entropy (128 bits)
      -> mnemonic (12 words, BIP39 English wordlist)
      -> seed (64 bytes via PBKDF2-HMAC-SHA512, 2048 iterations)
      -> SLIP-0010 m/44'/7999'/0'/0'/0' (all hardened)
      -> 32-byte Ed25519 private seed
      -> EVP_PKEY (Ed25519)

Coin type **7999** is used as an unregistered identifier in the
7000-7999 SLIP-44 developer range. Formal registration is deferred
until the project becomes production-ready.

### 3.2 Signatures

Transactions and messages are signed with Ed25519 (RFC 8032).
Signatures are 64 bytes. Verification uses the raw 32-byte public
key. The sign/verify endpoints are exposed at the HTTP layer as
`/api/wallet/sign` and `/api/wallet/verify`.

### 3.3 Storage Encryption

Wallet keys are stored encrypted with AES-256-GCM:

    plaintext: PEM-encoded Ed25519 private key (~119 bytes)
    ciphertext: salt(16) || iv(12) || tag(16) || ciphertext

The key is derived from the user PIN via PBKDF2-HMAC-SHA256 with
100,000 iterations and a 16-byte salt. Corruption or an incorrect
PIN produces a GCM tag mismatch, which aborts the unlock.

### 3.4 Proof of Work

Blocks are mined with SHA-256 based proof of work. The difficulty
is an integer count of leading zero bits on the block hash. A
minimum difficulty of 8 is enforced to prevent zero-work
degeneration. Difficulty is recalculated every 11 blocks using a
median of the intervals, with a target block time of 120 seconds.

## 4. Data Model

### 4.1 Block Header

    struct BlockHeader {
        std::uint32_t version;
        std::uint64_t height;
        Hash256 previous_hash;
        Hash256 merkle_root;
        Hash256 witness_root;
        std::uint64_t timestamp;
        std::uint64_t nonce;
        std::uint32_t difficulty;
    };

### 4.2 Genesis

Two canonical genesis blocks are defined:

- Mainnet: burn recipient `CAESAR_GENESIS_BURN`,
  hash `7e026bb394aff5047e026130bfe0a14d6fbaa971691be66305715f48f612a5bc`
- Testnet: burn recipient `CAESAR_TESTNET_GENESIS_BURN`,
  hash `ba67ed0363858fa4505fd0f6ffea1362f6e79b59ed4d0dd93323e4b549e38ec3`

Changing either constant is a hard fork.

### 4.3 Storage

The chain is stored in a single file `blockchain.dat` with an
append-only format. Every wallet operation that writes to disk is
atomic: a temporary file is written, fsynced, renamed, and the
parent directory is fsynced. This prevents partial or torn writes.

## 5. Network Layer

### 5.1 Transport

P2P communication uses TCP with a HELLO handshake that includes:

- Protocol version
- Network ID (1 = mainnet, 2 = testnet)
- Genesis hash
- Current chain height
- Peer listening port

Peers with mismatched genesis hashes are rejected.

### 5.2 Message Types

- `HELLO` / `HELLO_ACK` — handshake
- `GET_HEADERS` / `HEADERS` — chain sync
- `GET_BLOCK` / `BLOCK` — block retrieval
- `ANNOUNCE_BLOCK` — new block propagation
- `GET_MEMPOOL` / `MEMPOOL` — transaction pool
- `PING` / `PONG` — liveness

### 5.3 Propagation

When a node mines or receives a valid block, it announces it to all
connected peers. Nodes accept new chains only if they have strictly
greater cumulative work than the current chain.

## 6. Wallet

### 6.1 HTTP Endpoints

- `POST /api/auth/setup` — create a fresh HD wallet + PIN
- `POST /api/auth/unlock` — decrypt wallet.pem with PIN
- `POST /api/auth/recover` — restore wallet from mnemonic
- `POST /api/auth/encrypt-wallet` — encrypt existing plaintext wallet
- `POST /api/wallet/sign` — sign a message with the wallet key
- `POST /api/wallet/verify` — verify a signature
- `GET  /api/wallet/qr.svg` — QR code of the wallet address
- `GET  /api/wallet` — public key + address
- `GET  /api/balance` — current balance
- `GET  /api/blocks` — recent blocks as JSON
- `GET  /explorer` — HTML block explorer

### 6.2 UI

A single-page dark-themed interface is served at `/`. It includes:

- Node status (height, peers, mempool, uptime)
- Wallet balance and address
- QR code for receiving payments
- Send form
- Sign / Verify panel
- Log panel

## 7. Limitations

This is a **draft** whitepaper. Known limitations:

- No mainnet deployment. All activity is local or testnet.
- No exchange listings. CZR is not tradable on any platform.
- No formal security audit.
- No SLIP-44 registration.
- Mining is CPU-only and intended for testing.
- No light client (SPV) support.

## 8. Roadmap

- [ ] v0.2 — Multi-account wallets
- [ ] v0.3 — Transaction history view
- [ ] v0.4 — Address book
- [ ] v0.5 — Mobile-native wallet app
- [ ] v1.0 — Public testnet, external audit, SLIP-44 registration

## 9. License

MIT License (inherited from upstream dependencies).

## 10. References

- BIP39: https://github.com/bitcoin/bips/blob/master/bip-0039.mediawiki
- SLIP-0010: https://github.com/satoshilabs/slips/blob/master/slip-0010.md
- SLIP-0044: https://github.com/satoshilabs/slips/blob/master/slip-0044.md
- RFC 8032 (Ed25519): https://datatracker.ietf.org/doc/html/rfc8032
- qrcodegen: https://www.nayuki.io/page/qr-code-generator-library
- cpp-httplib: https://github.com/yhirose/cpp-httplib
