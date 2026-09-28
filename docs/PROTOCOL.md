# Caesar CZR Protocol Specification

Status: Draft. This document describes the current implementation in
`include/caesar/` and `src/`. It is subject to change as the project
matures. Anything not described here should be treated as an
implementation detail and not relied upon by third parties.

---

## 1. Overview

Caesar CZR is a proof-of-work blockchain. Nodes maintain a chain of
blocks, each containing a coinbase transaction and zero or more regular
transactions. The canonical chain is chosen by cumulative proof-of-work.
Transactions spend outputs of prior transactions according to a UTXO
model with witness-separated signatures.

---

## 2. Notation

- `u8`, `u32`, `u64` — little-endian unsigned integers.
- `Hash256` — 32-byte SHA-256 digest.
- `txid` — SHA256 of a transaction's non-witness serialization.
- `wtxid` — SHA256 of a transaction's full serialization including witness.
- `chainwork` — sum over blocks of `2^difficulty`. See section 9.

---

## 3. Genesis

Constructed by `build_canonical_genesis()` in `network_params.hpp`.

| Field | Value |
| --- | --- |
| version | 1 |
| height | 0 |
| previous_hash | all zeros |
| timestamp | 0 |
| nonce | 0 |
| difficulty | 0 |
| Transactions | exactly one |
| Inputs | none |
| Outputs | one output, amount = 1, recipient = `CAESAR_GENESIS_BURN` |
| Witness | empty |

The Mainnet genesis hash is:

    7e026bb394aff5047e026130bfe0a14d6fbaa971691be66305715f48f612a5bc

A node running on a network with a pinned genesis hash MUST reject any
incoming chain whose first block does not hash to that value. Changing
genesis on a live network is a hard fork.

---

## 4. Block format

### 4.1 Header

| Field | Type | Size |
| --- | --- | --- |
| version | u32 | 4 |
| height | u64 | 8 |
| previous_hash | Hash256 | 32 |
| merkle_root | Hash256 | 32 |
| witness_root | Hash256 | 32 |
| timestamp | u64 | 8 |
| nonce | u64 | 8 |
| difficulty | u32 | 4 |

Total header size: 128 bytes. This is derived at compile time inside
`BlockHeader::serialize_binary()` and used by the P2P Headers parser.

### 4.2 Merkle roots

- `merkle_root` — Merkle root of the `txid`s of all transactions.
- `witness_root` — Merkle root of the `wtxid`s of all transactions.

### 4.3 Transactions

A block's first transaction MUST be the coinbase. All others MUST NOT be
coinbases. `validate_coinbase_position_and_reward()` enforces both rules.

---

## 5. Transaction format

| Field | Type |
| --- | --- |
| version | u32 |
| inputs | list of TransactionInput |
| outputs | list of TransactionOutput |
| witness | TransactionWitnessSet |

### 5.1 TransactionInput

| Field | Type |
| --- | --- |
| previous_txid | Hash256 |
| output_index | u32 |

### 5.2 TransactionOutput

| Field | Type |
| --- | --- |
| amount | u64 |
| recipient | string |

### 5.3 Witness

Each input has a corresponding witness entry:

| Field | Type |
| --- | --- |
| public_key | string (hex) |
| signature | vector of u8 |

`validate_transaction_witness()` requires
`witness.inputs.size() == tx.inputs.size()` and validates each signature.

### 5.4 Base validation rules

`Transaction::validate()` enforces:

- version != 0
- inputs non-empty
- outputs non-empty
- each output amount > 0
- each output recipient non-empty
- sum of outputs does not overflow u64

UTXO rules (input must exist, not already spent, signature verifies) are
enforced by `apply_block_transactions_canonical()` in `chain_validator.hpp`.

---

## 6. Proof of work

Parameters in `consensus.hpp`:

- `CZR_POW_VERSION = 2`
- `CZR_POW_MEMORY_WORDS = 65536`
- `CZR_POW_ROUNDS = 4`

The function takes the header with nonce set to zero, plus a candidate
nonce, and returns a Hash256. A block's PoW is valid if the number of
leading zero bits is at least `difficulty`.

Genesis is exempt from PoW (difficulty zero, trivially valid).

---

## 7. Difficulty

### 7.1 Retarget window

Recalculated every `CZR_DIFFICULTY_WINDOW = 11` blocks. For a block at
position i:

- If i < 11, expected difficulty is inherited from the previous block.
- Otherwise, intervals between consecutive timestamps of the preceding
  11 blocks are computed, their median is taken, clamped to
  [target/4, target*4], and passed to `adjust_difficulty_window`.

`CZR_TARGET_BLOCK_TIME = 120` seconds.

### 7.2 Adjustment

Let observed be the clamped median, next start at current difficulty:

- If observed > target, halve observed and decrement next while
  observed >= 2*target and next > CZR_MIN_DIFFICULTY.
- If observed < target, double observed and increment next while
  observed*2 <= target and next < CZR_MAX_DIFFICULTY.

`CZR_INITIAL_MINING_DIFFICULTY = 12`. `CZR_MAX_DIFFICULTY = 256`.

### 7.3 Single source of truth

Every path calls one of two functions in `block.hpp`:

- `expected_next_difficulty(chain)` — block extending the tip.
- `expected_difficulty_at_position(chain, position)` — historical block.

They must not be duplicated elsewhere.

---

## 8. Timestamp rules

`timestamp_consensus.hpp` defines:

- `CZR_MAX_FUTURE_DRIFT = 7200` seconds.
- `CZR_MEDIAN_TIME_WINDOW = 11` blocks.

Median-time-past (MTP) is the median of the timestamps of the last up to
11 blocks. For even-sized windows the upper of the two middle values is
returned (Bitcoin convention).

`validate_block_timestamp_canonical()` requires:

- block.timestamp <= now + CZR_MAX_FUTURE_DRIFT
- block.timestamp > MTP(chain)

**The check is opt-in.** Production paths still use
`block.timestamp >= previous.timestamp`. Wiring the canonical check into
the validator, storage, and mining paths is a tracked follow-up.

---

## 9. Chainwork and chain selection

Each block contributes `2^difficulty`. `ChainWork` in `chain_work.hpp` is
an exact 512-bit integer.

A candidate chain replaces the current canonical chain iff:

- It passes full consensus validation (`validate_candidate_chain`).
- Its genesis matches the current chain's genesis.
- `cumulative_chain_work(candidate) > cumulative_chain_work(current)`.

Equal work does not replace. Shorter chains with more work do replace.

Chainwork is a selection metric, not a consensus substitute.

---

## 10. Subsidy and supply

`coinbase.hpp` defines `CZR_MAX_SUPPLY`, `CZR_INITIAL_SUBSIDY`, and
`block_subsidy(height)`. The subsidy is `CZR_INITIAL_SUBSIDY >> halvings`.

A coinbase at height h MUST:

- Be the first transaction.
- Have exactly one output.
- Have output.amount == block_subsidy(h).
- Have a non-empty recipient.

Total issuance is bounded by `validate_total_coinbase_issuance()`.

---

## 11. Storage

Chain stored in `<data_dir>/blockchain.dat`.

### 11.1 Format

| Field | Type |
| --- | --- |
| Magic | u32 (0x435A5231) |
| Format version | u32 (1) |
| Block count | u64 |
| Blocks | count x serialized block |

### 11.2 Write protocol

`BlockchainStorage::save()`:

1. Open `<path>.tmp` truncating.
2. Write magic, version, count, blocks.
3. `out.flush()`.
4. Close.
5. `fsync(temp)`.
6. `rename(temp, path)` — atomic on POSIX.
7. `fsync(parent directory)`.

If rename fails, the existing file is preserved. If fsync on the parent
returns EINVAL (some filesystems, e.g. f2fs on Android), it is treated as
best-effort.

### 11.3 Read protocol

`load()` verifies magic and version, reads each block, recomputes PoW, and
rejects the file if PoW differs from what was stored. Corrupted or
truncated files are rejected with a clear error. No recovery mode yet.

---

## 12. P2P protocol

### 12.1 Handshake

Hello exchange with:

| Field | Type |
| --- | --- |
| protocol_version | u32 |
| network_id | u32 |
| height | u64 |
| timestamp | u64 |
| user_agent | string |

`protocol_version` is currently 1. Mismatched version or network id is
rejected at handshake time.

### 12.2 Frame format

| Field | Type |
| --- | --- |
| Payload size | u32 |
| Message type | u8 |
| Payload | size bytes |

Max payload: `CZR_P2P_MAX_PAYLOAD = 4 MiB`.

Message type bounds are `[P2P_MESSAGE_TYPE_MIN, P2P_MESSAGE_TYPE_MAX]`,
derived from the enum in `p2p_protocol.hpp`. The parser does not use a
hardcoded numeric bound.

### 12.3 Message types

| Value | Name |
| --- | --- |
| 1 | Hello |
| 2 | Ping |
| 3 | Pong |
| 4 | GetHeaders |
| 5 | Headers |
| 6 | GetBlocks |
| 7 | Blocks |
| 8 | GetMempool |
| 9 | Transaction |
| 10 | GetTransaction |
| 11 | Reject |
| 12 | GetSyncBlocks |
| 13 | SyncBlocks |

**Note**: only a subset is currently handled by
`P2PRelay::handle_frame()`. Ping, Pong, GetMempool, GetTransaction, and
Reject are defined but not yet handled.

### 12.4 Headers wire format

| Field | Type |
| --- | --- |
| Count | u32 |
| Per header | u32 size, then size bytes of serialized BlockHeader |

The parser computes the expected per-header size at compile time from
`BlockHeader::serialize_binary().size()`. It does not hardcode a literal.

### 12.5 Blocks wire format

| Field | Type |
| --- | --- |
| Count | u32 |
| Per block | u32 size, then size bytes of serialized block |

Blocks use `Block::serialize_full_binary()`, which includes the witness
set.

---

## 13. Mempool

### 13.1 Admission

A transaction enters the mempool only if:

- It passes `Transaction::validate()`.
- Its inputs exist in the current chain UTXO set.
- Signatures verify.
- No input is already reserved by another mempool transaction.
- It fits within MAX_TRANSACTIONS and the byte limit.

### 13.2 Dependency ordering

`Mempool::ordered_transactions(chain_utxos)` returns transactions in
topological order: parents before children.

### 13.3 Post-reorg revalidation

When the canonical chain changes via `CaesarNode::replace_chain()`, the
mempool is revalidated by `revalidate_mempool_after_reorg()`:

1. Snapshot all transactions.
2. Clear the pool.
3. Rebuild the UTXO set from the new chain.
4. Reinsert transactions in dependency order.
5. Drop any that fail admission. Descendants are dropped too.

Policy: "drop invalid". Not Bitcoin's "reinsert disconnected blocks".

---

## 14. Threading

| Mutex | Protects |
| --- | --- |
| CaesarNode::chain_mutex_ (shared_ptr) | All storage accesses |
| CaesarNode::mempool_mutex_ | Mempool accesses |
| CaesarNode::lifecycle_mutex_ | start / stop |
| P2PServer::attempt_mutex_ | Connection rate limiting |
| P2PPeerManager::mutex_ | Peer table |
| P2PRelay::threads_mutex_ | Worker threads and running_ |
| P2PRelay::pending_mutex_ | Pending sync sessions |

CaesarNode and P2PRelay share the same chain_mutex_ via shared_ptr.

The TSan job in CI verifies no data races remain in tested paths.

---

## 15. Open issues

Known gaps between the current implementation and a production Mainnet:

- No RPC API.
- No persistent, encrypted, coin-selecting wallet.
- Mempool lacks fee market, eviction, expiration, orphan pool, and
  reinsertion after reorg.
- P2P lacks peer scoring, banning, address discovery, and seed nodes.
- Five message types defined but not handled.
- Block lookup is O(chain length); no block index or UTXO database.
- Mining is a synchronous single-block miner.
- Canonical timestamp rules are opt-in.
- Testnet genesis is not defined.

Users should not treat the current code as Mainnet-ready.

---

## 16. Change log

Protocol-visible commits:

- e5e3c7c — fix Headers wire format size check.
- 3bf59df — unify difficulty calculation across all paths.
- 49b31eb — separate difficulty for tip versus historical position.
- 39fa326 — pin canonical genesis and add network identity check.
- 708e2ab — add canonical timestamp rules (opt-in).
- c61a268 — make P2PTcpSocket fd atomic to remove a data race.
