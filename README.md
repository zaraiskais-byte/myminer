# Caesar CZR

A from-scratch proof-of-work blockchain written in C++20.

Caesar CZR implements a full node with:

- A memory-hard proof-of-work function (custom, version 2).
- A UTXO-based transaction model with witness data and Ed25519 signatures.
- An 11-block difficulty retarget window targeting 120-second blocks.
- A peer-to-peer layer with handshake, headers/blocks sync, and transaction relay.
- Chain selection by cumulative proof-of-work (chainwork), not by height.
- Atomic, crash-safe on-disk chain storage.
- A mempool with dependency tracking and post-reorg revalidation.

The project is under active development. It is not yet Mainnet-ready; see
[docs/PROTOCOL.md](docs/PROTOCOL.md) for the current consensus rules and the
gaps that remain.

---

## Table of contents

- [Building](#building)
- [Running a node](#running-a-node)
- [Mining](#mining)
- [Tests](#tests)
- [Continuous integration](#continuous-integration)
- [Project layout](#project-layout)
- [Consensus at a glance](#consensus-at-a-glance)
- [Network parameters](#network-parameters)
- [Fuzzing](#fuzzing)
- [Documentation](#documentation)
- [Contributing](#contributing)

---

## Building

Requirements:

- A C++20 compiler (Clang 14+ or GCC 11+)
- CMake 3.20+
- OpenSSL development headers (libssl-dev on Debian/Ubuntu, openssl on Termux)

On Debian / Ubuntu:

    sudo apt-get install -y clang cmake libssl-dev
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build -j2

On Termux (Android):

    pkg install clang cmake openssl
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build -j2

The build produces the `caesard` binary inside `build/`.

### Sanitizer builds

Address + UndefinedBehavior:

    cmake -S . -B sanitizer-build \
      -DCMAKE_CXX_COMPILER=clang++ \
      -DCAESAR_ENABLE_SANITIZERS=ON
    cmake --build sanitizer-build -j2
    ctest --test-dir sanitizer-build --output-on-failure

ThreadSanitizer (Clang only, cannot combine with the above):

    cmake -S . -B tsan-build \
      -DCMAKE_CXX_COMPILER=clang++ \
      -DCAESAR_ENABLE_TSAN=ON
    cmake --build tsan-build -j2
    ctest --test-dir tsan-build --output-on-failure

---

## Running a node

    ./build/caesard

By default the node creates a data directory named `data/` in the current
working directory, stores the blockchain in `data/blockchain.dat`, listens
on TCP port `18444`, and joins network id `1` (Mainnet identifier; see
[docs/PROTOCOL.md](docs/PROTOCOL.md)).

On first start the node writes the canonical genesis block and begins
listening for peers. Press Ctrl+C to stop.

---

## Mining

The current binary supports a single-block synchronous miner:

    ./build/caesard --mine

This mines exactly one block into the local chain and exits. It is a
development aid, not a production miner. A worker-thread pool, hashrate
reporting, and stale-block detection are planned.

---

## Tests

The project ships 59 unit and integration tests. Run them with:

    ctest --test-dir build --output-on-failure

Coverage includes consensus and difficulty retargeting, block and header
validation, transaction and witness serialization, UTXO accounting,
mempool admission and dependency ordering, chainwork arithmetic and
reorg selection, chain replacement atomicity, P2P framing and peer
lifecycle, storage atomicity and durability, genesis identity, and
timestamp consensus rules.

Some end-to-end tests mine real blocks and take several minutes.

---

## Continuous integration

Every push to `main` runs two jobs on GitHub Actions:

1. **build-test-sanitize-fuzz**
   - Clang build with the full unit and integration test suite.
   - ASan + UBSan build and test run.
   - Three libFuzzer targets (transaction, block, crypto/Merkle), each
     running 10,000 iterations.

2. **tsan-test**
   - Builds concurrency-sensitive tests with ThreadSanitizer.
   - Runs `CaesarP2PLifecycleRaceTest`, `CaesarNodeTest`, and
     `CaesarTwoNodeP2PTest` under TSan.
   - Catches data races that ASan and UBSan cannot observe.

---

## Project layout

    include/caesar/         Public headers, one per subsystem
    src/main.cpp            The caesard binary
    tests/                  Unit and integration tests
    fuzz/                   libFuzzer targets
    scripts/                Developer helper scripts
    .github/workflows/      CI configuration
    docs/                   Protocol and architecture documentation

Key headers:

| Header | Purpose |
| --- | --- |
| `block.hpp` | Block and header serialization, PoW helpers |
| `consensus.hpp` | PoW function, difficulty retarget, chainwork |
| `chain_validator.hpp` | Full block and chain validation |
| `chain_replacement.hpp` | Fork assembly and atomic commit |
| `chain_work.hpp` | 512-bit chainwork arithmetic |
| `coinbase.hpp` | Subsidy schedule and coinbase validation |
| `transaction.hpp` | Transaction, inputs, outputs, witness |
| `mempool.hpp` | Transaction pool with dependency ordering |
| `mempool_reorg.hpp` | Post-reorg mempool revalidation |
| `blockchain_storage.hpp` | Atomic, crash-safe on-disk chain |
| `network_params.hpp` | Network ids, genesis, identity check |
| `timestamp_consensus.hpp` | Median-time-past and future-drift rules |
| `p2p_*.hpp` | Handshake, framing, peer manager, relay |
| `node.hpp` | Top-level node lifecycle |

---

## Consensus at a glance

Full specification: [docs/PROTOCOL.md](docs/PROTOCOL.md).

- **PoW**: memory-hard, custom function (`CZR_POW_VERSION = 2`).
- **Genesis**: single burn transaction, difficulty 0, zeroed previous hash.
  The Mainnet genesis hash is pinned in `network_params.hpp`.
- **Difficulty**: retargeted every 11 blocks from the median of the last 11
  inter-block intervals. Target block time: 120 seconds. Initial
  difficulty: 12. Maximum: 256.
- **Chain selection**: cumulative chainwork, never height. A shorter fork
  with more work wins.
- **Timestamps**: canonical rules use median-time-past over 11 blocks plus
  a 2-hour future drift bound. These are implemented but opt-in.
- **Subsidy**: see `CZR_INITIAL_SUBSIDY` and `CZR_MAX_SUPPLY` in
  `coinbase.hpp`.
- **Witness**: `txid` excludes witness; `wtxid` includes it.

---

## Network parameters

| Parameter | Value |
| --- | --- |
| P2P default port | 18444 |
| Network id (Mainnet) | 1 |
| Network id (Testnet) | 2 |
| P2P protocol version | 1 |
| Target block time | 120 s |
| Difficulty window | 11 blocks |
| Initial difficulty | 12 |
| Maximum difficulty | 256 |
| MTP window | 11 blocks |
| Max future drift | 7200 s |
| Genesis burn recipient | `CAESAR_GENESIS_BURN` |
| Mainnet genesis hash | `7e026bb3…12a5bc` (see `network_params.hpp`) |

---

## Fuzzing

Three libFuzzer targets live under `fuzz/`:

- `transaction_fuzzer.cpp`
- `block_fuzzer.cpp`
- `crypto_fuzzer.cpp`

Build and run one manually:

    clang++ -std=c++20 \
      -fsanitize=fuzzer,address,undefined \
      -fno-omit-frame-pointer -fno-sanitize-recover=all \
      -Iinclude fuzz/transaction_fuzzer.cpp -lcrypto \
      -o /tmp/transaction-fuzzer
    /tmp/transaction-fuzzer -runs=100000 -max_len=4096

---

## Documentation

- [docs/PROTOCOL.md](docs/PROTOCOL.md) — full protocol specification.

---

## Contributing

Pull requests are welcome. Please keep the build green on both CI jobs,
add a regression test for every behavioural fix, and run `ctest` locally
before opening a PR.

---

## License

See the repository for license details.

## Wallet Recovery

Wallets are derived from a BIP39 mnemonic and can be fully recovered.

    # Set up a new wallet
    curl -X POST http://127.0.0.1:8443/api/auth/setup \
        -H "Content-Type: application/json" \
        -d '{"pin":"12345678"}'
    # Returns: {"status":"ok","mnemonic":"...","address":"CZ1..."}

    # Recover from mnemonic on a fresh device
    curl -X POST http://127.0.0.1:8443/api/auth/recover \
        -H "Content-Type: application/json" \
        -d '{"mnemonic":"...","new_pin":"87654321"}'
    # Returns: {"status":"ok","address":"CZ1..."} same address

Encrypt an existing plaintext wallet:

    curl -X POST http://127.0.0.1:8443/api/auth/encrypt-wallet \
        -H "Content-Type: application/json" \
        -d '{"pin":"12345678"}'

See SECURITY.md for the full threat model.
