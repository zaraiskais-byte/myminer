#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <caesar/block_builder.hpp>
#include <caesar/chain_replacement.hpp>
#include <caesar/blockchain_storage.hpp>
#include <caesar/mempool.hpp>
#include <caesar/mempool_reorg.hpp>
#include <caesar/network_params.hpp>
#include <caesar/ownership.hpp>
#include <caesar/p2p_relay.hpp>
#include <caesar/p2p_server.hpp>

namespace caesar {

class CaesarNode {
public:
    explicit CaesarNode(
        std::filesystem::path data_dir,
        std::uint16_t p2p_port = 18444,
        std::uint32_t network_id = 1)
        : data_dir_(std::move(data_dir)),
          chain_path_(data_dir_ / "blockchain.dat"),
          storage_(chain_path_),
          p2p_port_(p2p_port),
          network_id_(network_id),
          chain_mutex_(std::make_shared<std::mutex>()),
          relay_(server_, storage_, chain_mutex_) {
        relay_.set_chain_replacement_callback(
            [this](const std::vector<Block>& candidate) {
                return replace_chain(candidate);
            });

        relay_.set_transaction_callback(
            [this](const Transaction& tx) {
                return accept_transaction(tx).accepted();
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
            throw std::runtime_error(
                "Caesar node already running");

        ensure_chain();

        std::vector<Block> chain;
        {
            std::lock_guard<std::mutex> chain_lock(*chain_mutex_);
            chain = storage_.load();
        }

        server_.start(
            p2p_port_,
            "0.0.0.0",
            network_id_,
            chain.back().header.height);

        try {
            relay_.start();
        } catch (...) {
            server_.stop();
            throw;
        }

        running_ = true;
    }

    void stop() noexcept {
        std::lock_guard<std::mutex> lock(lifecycle_mutex_);

        if (!running_)
            return;

        running_ = false;

        relay_.stop();
        server_.stop();
    }

    bool running() const noexcept {
        return running_;
    }

    std::vector<Block> chain() const {
        std::lock_guard<std::mutex> lock(*chain_mutex_);
        return storage_.load();
    }

    std::size_t height() const {
        const auto current = chain();
        if (current.empty())
            return 0;

        return static_cast<std::size_t>(
            current.back().header.height);
    }

    bool replace_chain(const std::vector<Block>& candidate) {
        if (!running_)
            throw std::runtime_error(
                "cannot replace chain while node is stopped");

        std::lock_guard<std::mutex> lock(*chain_mutex_);

        const auto current = storage_.load();

        auto plan =
            prepare_chain_replacement(
                current,
                candidate);

        if (!plan)
            return false;

        // Persistence succeeds before the in-memory operation can
        // report success. The candidate has already been fully
        // validated and its UTXO set rebuilt by prepare_chain_replacement().
        storage_.replace(plan->chain);

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

    std::uint64_t connect_to_peer(
        const std::string& address,
        std::uint16_t port) {

        if (!running_)
            throw std::runtime_error(
                "cannot connect while node is stopped");

        return server_.connect_to_peer(
            address,
            port);
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
        std::lock_guard<std::mutex> lock(mempool_mutex_);
        return mempool_.size();
    }

    bool mempool_contains(const Hash256& txid) const {
        std::lock_guard<std::mutex> lock(mempool_mutex_);
        return mempool_.contains(txid);
    }

    MempoolValidationResult accept_transaction(
        const Transaction& tx) {

        if (!running_)
            throw std::runtime_error(
                "cannot accept transaction while node is stopped");

        MempoolValidationResult result;

        {
            std::lock_guard<std::mutex> chain_lock(*chain_mutex_);

            const auto current_chain =
                storage_.load();

            if (current_chain.empty())
                throw std::runtime_error(
                    "cannot accept transaction on empty blockchain");

            const UTXOSet utxos =
                rebuild_utxo_set(current_chain);

            if (is_coinbase_transaction(tx)) {
                result = {
                    MempoolRejectReason::InvalidTransaction
                };
            } else {
                std::lock_guard<std::mutex> mempool_lock(
                    mempool_mutex_);

                result = mempool_.accept(
                    tx,
                    utxos);
            }
        }

        /*
         * Local submissions and accepted peer transactions share the
         * same admission path. Relay only after the mempool accepted
         * the transaction and all chain/mempool locks are released.
         */
        if (result.accepted())
            relay_.announce_transaction(tx);

        return result;
    }

    void mine_one_block(
        const std::string& miner_recipient,
        std::uint64_t max_attempts = 1000000) {

        if (!running_)
            throw std::runtime_error(
                "cannot mine while node is stopped");

        if (miner_recipient.empty())
            throw std::runtime_error(
                "miner recipient is empty");

        std::lock_guard<std::mutex> chain_lock(*chain_mutex_);

        std::lock_guard<std::mutex> mempool_lock(
            mempool_mutex_);

        auto current_chain = storage_.load();

        if (current_chain.empty())
            throw std::runtime_error(
                "cannot mine on empty blockchain");

        const Block& previous =
            current_chain.back();

        const UTXOSet previous_utxos =
            rebuild_utxo_set(current_chain);

        const auto now =
            std::chrono::duration_cast<
                std::chrono::seconds>(
                std::chrono::system_clock::now()
                    .time_since_epoch())
                .count();

        const std::uint64_t timestamp =
            static_cast<std::uint64_t>(
                now < 0 ? 0 : now);

        const std::uint64_t block_timestamp =
            timestamp < previous.header.timestamp
                ? previous.header.timestamp
                : timestamp;

        const std::uint32_t difficulty =
            expected_next_difficulty(current_chain);

        Block candidate =
            BlockBuilder::build(
                previous,
                mempool_,
                miner_recipient,
                block_timestamp,
                difficulty,
                0,
                &previous_utxos);

        if (!BlockBuilder::mine(
                candidate,
                0,
                max_attempts)) {
            throw std::runtime_error(
                "mining failed");
        }

        if (!validate_block_consensus(
                candidate,
                current_chain,
                previous_utxos)) {
            throw std::runtime_error(
                "mined block failed consensus");
        }

        storage_.append(candidate);

        mempool_.clear();

        relay_.announce_block(candidate);
    }

private:
    void ensure_chain() {
        std::lock_guard<std::mutex> lock(*chain_mutex_);

        std::filesystem::create_directories(
            data_dir_);

        if (storage_.exists()) {
            (void)storage_.load();
            return;
        }

        const Block genesis = build_canonical_genesis();

        storage_.save({genesis});
    }

    std::filesystem::path data_dir_;
    std::filesystem::path chain_path_;

    mutable std::shared_ptr<std::mutex> chain_mutex_;
    mutable std::mutex lifecycle_mutex_;
    mutable std::mutex mempool_mutex_;

    BlockchainStorage storage_;
    Mempool mempool_;

    P2PServer server_;
    P2PRelay relay_;

    std::uint16_t p2p_port_;
    std::uint32_t network_id_;

    std::atomic<bool> running_{false};
};

} // namespace caesar
