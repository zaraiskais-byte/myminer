#pragma once

#include <chrono>
#include <fstream>
#include <cstdlib>
#include <string>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <caesar/block_builder.hpp>
#include <caesar/blockchain_storage.hpp>
#include <caesar/chain_replacement.hpp>
#include <caesar/mempool.hpp>
#include <caesar/mempool_reorg.hpp>
#include <caesar/network_params.hpp>
#include <caesar/ownership.hpp>
#include <caesar/p2p_relay.hpp>
#include <caesar/p2p_server.hpp>

namespace caesar {

/*
 * Diagnostic tracing.
 *
 * submit_pool_solution() used to unconditionally write ~20 std::cerr
 * lines per call. The pool backend posts a solution every few seconds
 * on an idle rig, so the log volume alone is enough to saturate a
 * mobile console, and there is no way to silence the traces without
 * editing the source and recompiling.
 *
 * CZR_NODE_TRACE is a compile-time diagnostic switch. It is disabled
 * by default. Define CZR_NODE_TRACE_ENABLED at build time to enable
 * the traces for a debugging session.
 *
 * Arguments are not evaluated when the macro is disabled, so callers
 * must not rely on side effects inside the argument list.
 */
#ifdef CZR_NODE_TRACE_ENABLED
#define CZR_NODE_TRACE(expr) do { std::cerr << expr << std::endl; } while (0)
#else
#define CZR_NODE_TRACE(expr) do { } while (0)
#endif

class CaesarNode {
   public:
    explicit CaesarNode(std::filesystem::path data_dir, std::uint16_t p2p_port = 18444,
                        std::uint32_t network_id = 1)
        : data_dir_(std::move(data_dir)),
          chain_path_(data_dir_ / "blockchain.dat"),
          storage_(chain_path_),
          p2p_port_(p2p_port),
          network_id_(network_id),
          chain_mutex_(std::make_shared<std::mutex>()),
          relay_(server_, storage_, chain_mutex_) {
        relay_.set_chain_replacement_callback(
            [this](const std::vector<Block>& candidate) { return replace_chain(candidate); });

        relay_.set_invalidate_cache_callback([this]() {
            std::lock_guard<std::mutex> lock(*chain_mutex_);
            chain_cache_valid_ = false;
        });

        relay_.set_transaction_callback(
            [this](const Transaction& tx) { return accept_transaction(tx).accepted(); });

        /*
         * Expose a read-only snapshot of the mempool to the relay so
         * it can answer GetMempool and GetTransaction from peers.
         * Both callbacks take mempool_mutex_ for the duration of the
         * read, so a peer request cannot observe a half-updated pool.
         */
        relay_.set_mempool_snapshot_callback([this]() {
            std::lock_guard<std::mutex> lock(mempool_mutex_);

            std::vector<Transaction> txs;
            txs.reserve(mempool_.size());

            for (const auto& entry : mempool_.transactions())
                txs.push_back(entry.second);

            return txs;
        });

        relay_.set_mempool_tx_callback([this](const Hash256& txid) -> std::optional<Transaction> {
            std::lock_guard<std::mutex> lock(mempool_mutex_);

            const Transaction* tx = mempool_.get(txid);
            if (!tx)
                return std::nullopt;

            return *tx;
        });
    }

    ~CaesarNode() {
        stop();
    }

    CaesarNode(const CaesarNode&) = delete;
    CaesarNode& operator=(const CaesarNode&) = delete;

    void start() {
        std::lock_guard<std::mutex> lock(lifecycle_mutex_);

        if (running_)
            throw std::runtime_error("Caesar node already running");

        ensure_chain();

        std::vector<Block> chain;
        {
            std::lock_guard<std::mutex> chain_lock(*chain_mutex_);
            chain = storage_.load();
        }

        /*
         * Network-identity gate: the first block of our local chain
         * must hash to the pinned genesis for the network we were
         * constructed with. If the storage on disk belongs to a
         * different network (Mainnet storage opened as Testnet, or
         * vice versa), refuse to start rather than silently serving
         * a foreign chain to peers.
         */
        {
            const Hash256* expected = genesis_hash_for_network(network_id_);

            if (!validate_genesis_network_identity(chain.front(), expected)) {
                throw std::runtime_error(
                    "storage contains a chain from a different "
                    "network (genesis hash mismatch)");
            }
        }

        height_snapshot_.store(chain.back().header.height, std::memory_order_relaxed);
        server_.start(p2p_port_, "0.0.0.0", network_id_, chain.back().header.height);

        try {
            relay_.start();
        } catch (...) {
            server_.stop();
            throw;
        }

        running_ = true;
    }

    void stop() noexcept {
    {
        std::lock_guard<std::mutex> lifecycle_lock(lifecycle_mutex_);

        if (!running_)
            return;

        running_ = false;
    }

    {
        std::lock_guard<std::mutex> mining_lock(mining_mutex_);

        if (mining_thread_.joinable())
            mining_thread_.join();
    }

    relay_.stop();
    server_.stop();
}

    bool running() const noexcept {
        return running_;
    }

    std::vector<Block> chain() const {
        std::lock_guard<std::mutex> lock(*chain_mutex_);
        if (!chain_cache_valid_) {
            chain_cache_ = storage_.load();
            chain_cache_valid_ = true;
        }
        return chain_cache_;
    }

    std::size_t height() const {
        std::unique_lock<std::mutex> lock(
            *chain_mutex_, std::try_to_lock);

        // Keep HTTP status and pool badge responsive while a block
        // is being validated or persisted under the chain mutex.
        if (!lock.owns_lock())
            return height_snapshot_.load(std::memory_order_relaxed);

        if (!chain_cache_valid_) {
            chain_cache_ = storage_.load();
            chain_cache_valid_ = true;
        }

        const std::size_t current_height = chain_cache_.empty()
            ? 0
            : static_cast<std::size_t>(chain_cache_.back().header.height);

        height_snapshot_.store(current_height, std::memory_order_relaxed);
        return current_height;
    }

    bool replace_chain(const std::vector<Block>& candidate) {
        if (!running_)
            throw std::runtime_error("cannot replace chain while node is stopped");

        std::lock_guard<std::mutex> lock(*chain_mutex_);

        const auto current = storage_.load();

        auto plan = prepare_chain_replacement(current, candidate);

        if (!plan)
            return false;

        // Persistence succeeds before the in-memory operation can
        // report success. The candidate has already been fully
        // validated and its UTXO set rebuilt by prepare_chain_replacement().
        storage_.replace(plan->chain);
        chain_cache_valid_ = false;

        // Revalidate the mempool against the new UTXO set. Any
        // transaction whose inputs no longer exist under the new
        // chain, or whose parent was itself rejected, is dropped.
        // This runs under mempool_mutex_ so concurrent readers of
        // mempool_size() / mempool_contains() see a consistent view.
        {
            std::lock_guard<std::mutex> mempool_lock(mempool_mutex_);
            revalidate_mempool_after_reorg(mempool_, plan->chain);
        }

        return true;
    }


    std::size_t peer_count() const noexcept {
        return server_.peer_count();
    }

    std::uint64_t connect_to_peer(const std::string& address, std::uint16_t port) {
        if (!running_)
            throw std::runtime_error("cannot connect while node is stopped");

        return server_.connect_to_peer(address, port);
    }

    /*
     * Thread-safe read-only accessors.
     *
     * Earlier revisions exposed `Mempool& mempool()` and
     * `const Mempool& mempool()`. The non-const overload allowed any
     * external caller to mutate the mempool without holding
     * mempool_mutex_, bypassing accept_transaction(). The const
     * overload returned a reference whose lifetime extended past any
     * internal lock, so concurrent writes could still race against
     * readers that walked the returned reference.
     *
     * These accessors take mempool_mutex_ internally for the duration
     * of the read, so callers cannot observe a half-updated mempool.
     */
    std::size_t mempool_size() const {
        // Non-blocking: submit_pool_solution holds mempool_mutex_ for the whole
        // block validation, so return the last known size if the lock is busy.
        std::unique_lock<std::mutex> lock(mempool_mutex_, std::try_to_lock);
        if (!lock.owns_lock())
            return mempool_size_snapshot_.load(std::memory_order_relaxed);
        const std::size_t n = mempool_.size();
        mempool_size_snapshot_.store(n, std::memory_order_relaxed);
        return n;
    }

    bool mempool_contains(const Hash256& txid) const {
        std::lock_guard<std::mutex> lock(mempool_mutex_);
        return mempool_.contains(txid);
    }

    MempoolValidationResult accept_transaction(const Transaction& tx) {
        const auto czr_a0 = std::chrono::steady_clock::now();
        auto czr_el = [&]() -> long long { return (long long)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - czr_a0).count(); };
        auto czr_wr = [](const std::string& m) { try { const char* h = std::getenv("HOME"); std::ofstream f(std::string(h ? h : ".") + "/czr-send.log", std::ios::app); f << "[accept] " << m << "\n"; } catch (...) {} };
        long long czr_l1 = 0, czr_l2 = 0, czr_l3 = 0;
        const char* czr_path = "full";
        if (!running_)
            throw std::runtime_error("cannot accept transaction while node is stopped");

        MempoolValidationResult result;

        {
            std::lock_guard<std::mutex> chain_lock(*chain_mutex_);
            czr_l1 = czr_el();

            const auto current_chain = storage_.load();
            czr_l2 = czr_el();

            if (current_chain.empty())
                throw std::runtime_error("cannot accept transaction on empty blockchain");

            const Hash256 czr_tip = current_chain.back().hash();
            if (utxo_cache_valid_ && !(utxo_cache_tip_ != czr_tip)) {
                czr_path = "hit";
            } else {
                bool czr_done = false;
                if (utxo_cache_valid_) {
                    const std::size_t czr_n = current_chain.size();
                    const std::size_t czr_lo = czr_n > 65 ? czr_n - 65 : 0;
                    for (std::size_t czr_i = czr_n - 1; czr_i-- > czr_lo;) {
                        if (current_chain[czr_i].hash() != utxo_cache_tip_)
                            continue;
                        try {
                            UTXOSet czr_t = utxo_cache_;
                            for (std::size_t czr_j = czr_i + 1; czr_j < czr_n; ++czr_j)
                                czr_t = apply_block_transactions(current_chain[czr_j], czr_t);
                            utxo_cache_ = std::move(czr_t);
                            czr_done = true;
                        } catch (...) {
                            czr_done = false;
                        }
                        break;
                    }
                }
                if (czr_done)
                    czr_path = "incr";
                else
                    utxo_cache_ = rebuild_utxo_set(current_chain);
                utxo_cache_tip_ = czr_tip;
                utxo_cache_valid_ = true;
            }
            const UTXOSet& utxos = utxo_cache_;
            czr_l3 = czr_el();

            if (is_coinbase_transaction(tx)) {
                result = {MempoolRejectReason::InvalidTransaction};
            } else {
                std::lock_guard<std::mutex> mempool_lock(mempool_mutex_);

                result = mempool_.accept(tx, utxos);
            }
        }

        /*
         * Local submissions and accepted peer transactions share the
         * same admission path. Relay only after the mempool accepted
         * the transaction and all chain/mempool locks are released.
         */
        if (result.accepted())
            { const long long czr_l4 = czr_el(); relay_.announce_transaction(tx); czr_wr("accept path=" + std::string(czr_path) + " lock_ms=" + std::to_string(czr_l1) + " load_ms=" + std::to_string(czr_l2 - czr_l1) + " rebuild_ms=" + std::to_string(czr_l3 - czr_l2) + " mempool_ms=" + std::to_string(czr_l4 - czr_l3) + " relay_ms=" + std::to_string(czr_el() - czr_l4)); }

        return result;
    }

    bool submit_transaction(const Transaction& tx) {
        /*
         * Legacy alias for accept_transaction().
         *
         * Previous revisions duplicated the mempool admission path
         * here without:
         *   - checking running_,
         *   - checking that the chain is non-empty,
         *   - rejecting coinbase transactions,
         *   - announcing the accepted transaction to peers.
         *
         * The last point was a liveness bug: a transaction
         * submitted through this method (which is what the HTTP
         * RPC path uses) entered the local mempool but never
         * propagated to any peer.
         *
         * Route through accept_transaction() so both entry points
         * share the same admission policy and the same relay
         * behavior.
         */
        return accept_transaction(tx).accepted();
    }

    std::vector<Transaction> mempool_transactions() const {
        std::lock_guard<std::mutex> lock(mempool_mutex_);
        std::vector<Transaction> out;
        for (const auto& entry : mempool_.transactions()) {
            out.push_back(entry.second);
        }
        return out;
    }

    bool request_mine_one_block(
        const std::string& miner_recipient,
        std::uint64_t max_attempts = 1000000) {
    {
        std::lock_guard<std::mutex> lifecycle_lock(lifecycle_mutex_);

        if (!running_)
            throw std::runtime_error("cannot mine while node is stopped");

        if (miner_recipient.empty())
            throw std::runtime_error("miner recipient is empty");

        bool expected = false;
        if (!mining_job_active_.compare_exchange_strong(
                expected, true,
                std::memory_order_acq_rel,
                std::memory_order_acquire)) {
            return false;
        }
    }

    std::lock_guard<std::mutex> mining_lock(mining_mutex_);

    if (mining_thread_.joinable())
        mining_thread_.join();

    try {
        mining_thread_ = std::thread(
            [this, miner_recipient, max_attempts]() {
                try {
                    mine_one_block(miner_recipient, max_attempts);
                } catch (...) {
                    // The HTTP request has already returned.
                    // Mining failures are contained here.
                }

                mining_job_active_.store(false, std::memory_order_release);
            });
    } catch (...) {
        mining_job_active_.store(false, std::memory_order_release);
        throw;
    }

    return true;
}

void mine_one_block(const std::string& miner_recipient,
                        std::uint64_t max_attempts = 1000000) {
        if (!running_)
            throw std::runtime_error("cannot mine while node is stopped");

        if (miner_recipient.empty())
            throw std::runtime_error("miner recipient is empty");

        Block candidate;
        std::vector<Block> mining_chain;
        UTXOSet mining_utxos;

        /*
         * Snapshot the chain and mempool while holding their locks.
         * The expensive PoW operation must never run while either lock
         * is held, otherwise P2P sync and transaction admission can be
         * blocked for many seconds on a mobile CPU.
         */
        {
            std::lock_guard<std::mutex> chain_lock(*chain_mutex_);
            std::lock_guard<std::mutex> mempool_lock(mempool_mutex_);

            mining_chain = storage_.load();

            if (mining_chain.empty())
                throw std::runtime_error("cannot mine on empty blockchain");

            mining_utxos = rebuild_utxo_set(mining_chain);

            const Block& previous = mining_chain.back();

            const auto now = std::chrono::duration_cast<std::chrono::seconds>(
                                 std::chrono::system_clock::now().time_since_epoch())
                                 .count();

            const std::uint64_t timestamp =
                static_cast<std::uint64_t>(now < 0 ? 0 : now);

            const std::uint64_t block_timestamp =
                timestamp < previous.header.timestamp
                    ? previous.header.timestamp
                    : timestamp;

            const std::uint32_t difficulty =
                expected_next_difficulty(mining_chain);

            candidate = BlockBuilder::build(
                previous,
                mempool_,
                miner_recipient,
                block_timestamp,
                difficulty,
                0,
                &mining_utxos);
        }

        /*
         * PoW is intentionally performed without chain/mempool locks.
         * Consensus parameters and the PoW algorithm are unchanged.
         */
        if (!BlockBuilder::mine(candidate, 0, max_attempts))
            throw std::runtime_error("mining failed");

        /*
         * Reacquire the locks and verify that the chain tip used to build
         * the candidate is still current. A competing P2P block or reorg
         * invalidates the candidate and it must never be appended.
         */
        {
            std::lock_guard<std::mutex> chain_lock(*chain_mutex_);
            std::lock_guard<std::mutex> mempool_lock(mempool_mutex_);

            const auto current_chain = storage_.load();

            if (current_chain.empty())
                throw std::runtime_error("blockchain became empty while mining");

            const Block& current_previous = current_chain.back();

            if (candidate.header.previous_hash != current_previous.hash() ||
                candidate.header.height != current_previous.header.height + 1) {
                throw std::runtime_error(
                    "mined block became stale; chain advanced during mining");
            }

            const UTXOSet current_utxos = rebuild_utxo_set(current_chain);

            if (!validate_block_consensus(
                    candidate, current_chain, current_utxos)) {
                throw std::runtime_error("mined block failed consensus");
            }

            storage_.append(candidate);
            chain_cache_valid_ = false;

            mempool_.clear();
        }

        /*
         * Relay only after all chain/mempool locks are released.
         */
        relay_.announce_block(candidate);
    }

    Block build_pool_candidate(const std::string& miner_recipient) {
        if (!running_)
            throw std::runtime_error("cannot build candidate while node is stopped");
        if (miner_recipient.empty())
            throw std::runtime_error("miner recipient is empty");

        std::vector<Block> current_chain;

        /*
         * Load a consistent chain snapshot under the chain lock.
         * Expensive UTXO reconstruction and candidate construction
         * happen after releasing the chain lock.
         */
        {
            std::lock_guard<std::mutex> chain_lock(*chain_mutex_);
            current_chain = storage_.load();
        }

        if (current_chain.empty())
            throw std::runtime_error("cannot mine on empty blockchain");

        const Block& previous = current_chain.back();
        const UTXOSet previous_utxos = rebuild_utxo_set(current_chain);

        const auto now = std::chrono::duration_cast<std::chrono::seconds>(
                             std::chrono::system_clock::now().time_since_epoch())
                             .count();

        const std::uint64_t timestamp =
            static_cast<std::uint64_t>(now < 0 ? 0 : now);

        const std::uint64_t block_timestamp =
            timestamp < previous.header.timestamp
                ? previous.header.timestamp
                : timestamp;

        const std::uint32_t difficulty =
            expected_next_difficulty(current_chain);

        Mempool mempool_snapshot;
        {
            std::lock_guard<std::mutex> mempool_lock(mempool_mutex_);
            mempool_snapshot = mempool_;
        }

        /*
         * The returned candidate is based on snapshots. Submission
         * must still reject stale previous hashes/heights and validate
         * PoW and consensus before appending.
         */
        return BlockBuilder::build(
            previous,
            mempool_snapshot,
            miner_recipient,
            block_timestamp,
            difficulty,
            0,
            &previous_utxos);
    }

    bool submit_pool_solution(Block candidate) {
        CZR_NODE_TRACE("[submit] enter, height=" << candidate.header.height
                  << " nonce=" << candidate.header.nonce
                  << " diff=" << candidate.header.difficulty);

        if (!running_)
            throw std::runtime_error(
                "cannot submit solution while node is stopped");

        {
            const auto chain_wait_start = std::chrono::steady_clock::now();
            CZR_NODE_TRACE("[submit:lock] chain waiting");
            std::lock_guard<std::mutex> chain_lock(*chain_mutex_);
            const auto chain_wait_ms =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - chain_wait_start).count();
            CZR_NODE_TRACE("[submit:lock] chain acquired wait_ms="
                      << chain_wait_ms);

            const auto mempool_wait_start = std::chrono::steady_clock::now();
            CZR_NODE_TRACE("[submit:lock] mempool waiting");
            std::lock_guard<std::mutex> mempool_lock(mempool_mutex_);
            const auto mempool_wait_ms =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - mempool_wait_start).count();
            CZR_NODE_TRACE("[submit:lock] mempool acquired wait_ms="
                      << mempool_wait_ms);

            CZR_NODE_TRACE("[submit:1] storage.load begin");
            auto current_chain = storage_.load();
            CZR_NODE_TRACE("[submit:2] storage.load done blocks="
                      << current_chain.size());

            if (current_chain.empty())
                throw std::runtime_error(
                    "cannot submit on empty blockchain");

            const Block& previous = current_chain.back();

            if (candidate.header.previous_hash != previous.hash()) {
                CZR_NODE_TRACE("[FAIL-PREVHASH]");
                return false;
            }

            if (candidate.header.height !=
                previous.header.height + 1) {
                CZR_NODE_TRACE("[FAIL-HEIGHT] cand="
                          << candidate.header.height
                          << " want="
                          << (previous.header.height + 1));
                return false;
            }

            CZR_NODE_TRACE("[submit:3] validate_pow begin");
            const bool submit_pow_ok = candidate.validate_pow();
            CZR_NODE_TRACE("[submit:4] validate_pow done ok="
                      << submit_pow_ok);
            if (!submit_pow_ok) {
                CZR_NODE_TRACE("FAILPOW h="
                          << candidate.header.height);
                return false;
            }

            CZR_NODE_TRACE("[submit:5] rebuild_utxo begin");
            const UTXOSet previous_utxos =
                rebuild_utxo_set(current_chain);
            CZR_NODE_TRACE("[submit:6] rebuild_utxo done");

            CZR_NODE_TRACE("[submit:7] consensus begin");
            const bool submit_consensus_ok = validate_block_consensus(
                    candidate, current_chain, previous_utxos);
            CZR_NODE_TRACE("[submit:8] consensus done ok="
                      << submit_consensus_ok);
            if (!submit_consensus_ok) {
                return false;
            }

            const auto append_start = std::chrono::steady_clock::now();
            CZR_NODE_TRACE("[submit:9] append begin height="
                      << candidate.header.height);
            storage_.append(candidate, true);
            const auto append_ms =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - append_start).count();
            CZR_NODE_TRACE("[submit:10] append done duration_ms="
                      << append_ms);
            chain_cache_valid_ = false;
            mempool_.clear();
        }

        /*
         * Relay only after all chain/mempool locks are released.
         * announce_block() may acquire relay/storage-related locks.
         */
        CZR_NODE_TRACE("[submit:11] relay begin");
        relay_.announce_block(candidate);
        CZR_NODE_TRACE("[submit:12] relay done");

        CZR_NODE_TRACE("[submit] returning true");
        return true;
    }

    void request_sync_from_peer(std::uint64_t peer_id) {
        std::vector<Hash256> locator;
        {
            std::lock_guard<std::mutex> lock(*chain_mutex_);
            if (!chain_cache_valid_) {
                chain_cache_ = storage_.load();
                chain_cache_valid_ = true;
            }
            if (!chain_cache_.empty()) {
                locator.push_back(chain_cache_.back().hash());
                size_t i = chain_cache_.size();
                while (i > 1 && locator.size() < 10) {
                    i = i / 2;
                    if (i < chain_cache_.size()) {
                        locator.push_back(chain_cache_[i].hash());
                    } else {
                        break;
                    }
                }
            }
        }
        if (locator.empty()) locator.push_back(Hash256{});
        relay_.send_get_headers(peer_id, locator);
    }

   private:
    void ensure_chain() {
        std::lock_guard<std::mutex> lock(*chain_mutex_);

        std::filesystem::create_directories(data_dir_);

        if (storage_.exists()) {
            (void)storage_.load();
            return;
        }

        const Block genesis =
            (network_id_ == NETWORK_TESTNET) ? build_testnet_genesis() : build_canonical_genesis();

        storage_.save({genesis});
    }

    std::filesystem::path data_dir_;
    std::filesystem::path chain_path_;

    mutable std::shared_ptr<std::mutex> chain_mutex_;
    mutable std::mutex lifecycle_mutex_;
    mutable std::mutex mempool_mutex_;

    BlockchainStorage storage_;
    Mempool mempool_;

    mutable std::vector<Block> chain_cache_;
    mutable bool chain_cache_valid_ = false;
    mutable UTXOSet utxo_cache_;
    mutable Hash256 utxo_cache_tip_{};
    mutable bool utxo_cache_valid_ = false;
    mutable std::atomic<std::size_t> height_snapshot_{0};
    mutable std::atomic<std::size_t> mempool_size_snapshot_{0};

    P2PServer server_;
    P2PRelay relay_;

    std::uint16_t p2p_port_;
    std::uint32_t network_id_;

    std::atomic<bool> running_{false};
    std::mutex mining_mutex_;
    std::thread mining_thread_;
    std::atomic<bool> mining_job_active_{false};
};

} // namespace caesar
