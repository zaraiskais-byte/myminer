# Architecture

## Overview

Caesar CZR is split into three executables:

1. `caesard` — reference node (P2P only)
2. `caesar_web` — node + wallet + pool + dashboard (HTTP)
3. `caesar_worker` — external mining client

All three share the same header-only core in `include/caesar/`.

## Core Components

### Blockchain

- `block.hpp` — block and header structures, serialization
- `block_builder.hpp` — assemble a candidate block from mempool
- `blockchain_storage.hpp` — persistent chain on disk
- `chain_validator.hpp` — consensus rules
- `chain_replacement.hpp` — reorg handling
- `chain_work.hpp` — cumulative work calculation

### Consensus

- `consensus.hpp` — PoW hash, difficulty adjustment, `mine_pow()`
- `crypto.hpp` — hashing primitives
- `signature.hpp` — Ed25519 keypairs and signing
- `transaction_signature.hpp` — domain-separated input signing

### Network

- `p2p_server.hpp` — TCP listener
- `p2p_connection.hpp` — framed connection
- `p2p_handshake.hpp` — version exchange
- `p2p_relay.hpp` — block and transaction propagation
- `p2p_peer_manager.hpp` — peer tracking

### Wallet

- `wallet.hpp` — Ed25519 wallet
- `wallet_hd.hpp` — hierarchical deterministic derivation
- `bip39.hpp` / `slip10.hpp` — mnemonic and derivation
- `persistent_wallet.hpp` — disk persistence
- `secure_wallet.hpp` — AES-256-GCM encryption

### Pool

Pool logic lives in `http_rpc.hpp`:

- Worker registry (in-memory + persisted)
- Job builder (delegates to `CaesarNode::build_pool_candidate`)
- Share verification (checks PoW against share difficulty)
- Solution submission (delegates to `CaesarNode::submit_pool_solution`)
- Payout engine (`pool_execute_payout`)
- Auto-payout trigger (checked on every `/api/pool/stats` call)

### HTTP Layer

Served by `httplib.h` (header-only). Routes are registered in
`HttpRpcServer::HttpRpcServer()`.

## Data Flow

### Mining (pool)

    worker  ->  /api/pool/job       (GET)
            <-  {job_id, header_hex, share_difficulty, block_difficulty}

    worker  ->  (search for nonce locally)

    worker  ->  /api/pool/submit    (POST)
            <-  {credited, is_block, block_added}

    if is_block:
        pool -> CaesarNode::submit_pool_solution
        pool -> storage_.append
        pool -> relay_.announce_block

### Payout

    trigger (manual or auto)
        -> collect_my_utxos()
        -> build Transaction with N outputs
        -> sign each input with pool wallet
        -> node_.submit_transaction
        -> record in g_pool_payouts
        -> persist to pool_state.txt

## Concurrency

- `chain_mutex_` guards the blockchain
- `mempool_mutex_` guards the mempool
- `g_pool_mutex` guards pool state
- `auth_mutex_` guards session tokens

Lock ordering: always acquire `chain_mutex_` before `mempool_mutex_`.
Pool mutex is held independently.

## Persistence

- `blockchain.dat` — binary serialized chain
- `wallet.pem` — PEM-encoded Ed25519 key (optionally AES-encrypted)
- `pin.hash` — salt + PBKDF2 hash
- `session.txt` — current session token
- `pool_state.txt` — plain-text pool state

All files live in the `--data` directory.
