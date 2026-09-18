#pragma once

#include <atomic>
#include <cstdint>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

#include <caesar/block.hpp>
#include <caesar/blockchain_storage.hpp>
#include <caesar/p2p_connection.hpp>
#include <caesar/p2p_frame.hpp>
#include <caesar/p2p_peer_manager.hpp>
#include <caesar/p2p_protocol.hpp>
#include <caesar/p2p_server.hpp>
#include <caesar/serialization.hpp>

namespace caesar {

// Wire format for Blocks payload:
//   [u32 count]
//   count * [u32 block_size][block_size bytes (Block::serialize_full_binary)]
class P2PRelay {
public:
    P2PRelay(
        P2PServer& server,
        BlockchainStorage& storage)
        : server_(server),
          storage_(storage) {}

    ~P2PRelay() { stop(); }

    P2PRelay(const P2PRelay&) = delete;
    P2PRelay& operator=(const P2PRelay&) = delete;

    void start() {
        if (running_)
            return;

        running_ = true;

        server_.peers().set_peer_added_callback(
            [this](std::uint64_t id) {
                on_peer_added(id);
            });
    }

    void stop() noexcept {
        if (!running_)
            return;

        running_ = false;

        server_.peers().set_peer_added_callback({});

        std::vector<std::thread> threads;

        {
            std::lock_guard<std::mutex> lock(threads_mutex_);
            threads.swap(threads_);
        }

        for (auto& t : threads) {
            if (t.joinable())
                t.join();
        }
    }

    void announce_block(const Block& block) {
        server_.peers().broadcast(
            make_blocks_frame(block));
    }

private:
    static P2PFrame make_blocks_frame(
        const Block& block) {

        const auto encoded =
            block.serialize_full_binary();

        if (encoded.size() >
            static_cast<std::size_t>(
                std::numeric_limits<std::uint32_t>::max())) {
            throw std::runtime_error(
                "serialized block too large");
        }

        BinaryWriter writer;
        writer.write_u32(1);
        writer.write_u32(
            static_cast<std::uint32_t>(
                encoded.size()));
        writer.write_bytes(encoded);

        P2PFrame frame;
        frame.type = P2PMessageType::Blocks;
        frame.payload = writer.data();
        return frame;
    }

    void on_peer_added(std::uint64_t id) {
        if (!running_)
            return;

        std::lock_guard<std::mutex> lock(threads_mutex_);
        threads_.emplace_back(
            [this, id]() { peer_loop(id); });
    }

    void peer_loop(std::uint64_t id) {

        auto connection =
            server_.peers().connection(id);

        if (!connection)
            return;

        while (running_ &&
               server_.peers().contains(id)) {

            try {
                const P2PFrame frame =
                    connection->receive_frame();

                handle_frame(frame);

            } catch (...) {
                break;
            }
        }

        if (server_.peers().contains(id))
            server_.peers().remove_peer(id);
    }

    void handle_frame(const P2PFrame& frame) {

        switch (frame.type) {

            case P2PMessageType::Blocks:
                handle_blocks(frame.payload);
                break;

            default:
                break;
        }
    }

    void handle_blocks(
        const std::vector<std::uint8_t>& payload) {

        BinaryReader reader(payload);

        const std::uint32_t count =
            reader.read_u32();

        if (count == 0 || count > 1024)
            throw std::runtime_error(
                "invalid relayed block count");

        for (std::uint32_t i = 0; i < count; ++i) {

            const std::uint32_t size =
                reader.read_u32();

            if (size > CZR_P2P_MAX_PAYLOAD)
                throw std::runtime_error(
                    "relayed block too large");

            const auto data =
                reader.read_bytes(size);

            const Block block =
                Block::deserialize_full(data);

            try {
                std::lock_guard<std::mutex> lock(storage_mutex_);

                if (!storage_.exists())
                    throw std::runtime_error(
                        "cannot relay block without local chain");

                const auto chain = storage_.load();
                const auto previous_utxos =
                    rebuild_utxo_set(chain);

                if (!validate_block_consensus(
                        block,
                        chain,
                        previous_utxos)) {
                    throw std::runtime_error(
                        "relayed block failed consensus validation");
                }

                storage_.append(block);
            } catch (...) {
                continue;
            }

            server_.peers().broadcast(
                make_blocks_frame(block));
        }

        if (!reader.empty())
            throw std::runtime_error(
                "trailing bytes in relayed blocks payload");
    }

    P2PServer& server_;
    BlockchainStorage& storage_;

    std::atomic<bool> running_{false};

    std::mutex threads_mutex_;
    std::vector<std::thread> threads_;

    std::mutex storage_mutex_;
};

} // namespace caesar
