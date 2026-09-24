#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <caesar/block_builder.hpp>
#include <caesar/chain_replacement.hpp>
#include <caesar/blockchain_storage.hpp>
#include <caesar/mempool.hpp>
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
          relay_(server_, storage_) {
        relay_.set_chain_replacement_callback(
            [this](const std::vector<Block>& candidate) {
                return replace_chain(candidate);
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

        const auto chain = storage_.load();

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
        std::lock_guard<std::mutex> lock(chain_mutex_);
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

        std::lock_guard<std::mutex> lock(chain_mutex_);

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

    Mempool& mempool() noexcept {
        return mempool_;
    }

    const Mempool& mempool() const noexcept {
        return mempool_;
    }

    MempoolValidationResult accept_transaction(
        const Transaction& tx) {

        if (!running_)
            throw std::runtime_error(
                "cannot accept transaction while node is stopped");

        std::lock_guard<std::mutex> chain_lock(
            chain_mutex_);

        const auto current_chain =
            storage_.load();

        if (current_chain.empty())
            throw std::runtime_error(
                "cannot accept transaction on empty blockchain");

        const UTXOSet utxos =
            rebuild_utxo_set(current_chain);

        if (is_coinbase_transaction(tx)) {
            return {
                MempoolRejectReason::InvalidTransaction
            };
        }

        if (!validate_transaction_witness(
                tx,
                utxos)) {
            return {
                MempoolRejectReason::InvalidTransaction
            };
        }

        std::lock_guard<std::mutex> mempool_lock(
            mempool_mutex_);

        return mempool_.accept(
            tx,
            utxos);
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

        std::lock_guard<std::mutex> chain_lock(
            chain_mutex_);

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
            (previous.header.height == 0 &&
             previous.header.difficulty == 0)
                ? CZR_INITIAL_MINING_DIFFICULTY
                : previous.header.difficulty;

        Block candidate =
            BlockBuilder::build(
                previous,
                mempool_,
                miner_recipient,
                block_timestamp,
                difficulty);

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
        std::lock_guard<std::mutex> lock(chain_mutex_);

        std::filesystem::create_directories(
            data_dir_);

        if (storage_.exists()) {
            (void)storage_.load();
            return;
        }

        Block genesis;

        genesis.header.version = 1;
        genesis.header.height = 0;
        genesis.header.previous_hash = {};
        genesis.header.timestamp = 0;
        genesis.header.nonce = 0;
        genesis.header.difficulty = 0;

        Transaction genesis_tx;

        genesis_tx.outputs.push_back(
            TransactionOutput{
                1,
                "CAESAR_GENESIS_BURN"
            });

        genesis.transactions.push_back(
            genesis_tx);

        genesis.update_merkle_root();

        storage_.save({genesis});
    }

    std::filesystem::path data_dir_;
    std::filesystem::path chain_path_;

    mutable std::mutex chain_mutex_;
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
