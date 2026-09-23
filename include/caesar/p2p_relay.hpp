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

    static P2PFrame make_get_headers_frame(
        const std::vector<Hash256>& locator_hashes) {

        if (locator_hashes.empty() || locator_hashes.size() > 32)
            throw std::runtime_error(
                "invalid header locator count");

        BinaryWriter writer;
        writer.write_u32(
            static_cast<std::uint32_t>(locator_hashes.size()));

        for (const auto& hash : locator_hashes)
            for (const auto byte : hash)
                writer.write_u8(byte);

        P2PFrame frame;
        frame.type = P2PMessageType::GetHeaders;
        frame.payload = writer.data();
        return frame;
    }

    void request_headers(std::uint64_t id) {
        auto connection = server_.peers().connection(id);
        if (!connection)
            return;

        if (!storage_.exists())
            return;

        const auto chain = storage_.load();
        if (chain.empty())
            return;

        std::vector<Hash256> locators;
        locators.push_back(chain.back().hash());

        connection->send_frame(
            make_get_headers_frame(locators));
    }

    void on_peer_added(std::uint64_t id) {
        if (!running_)
            return;

        std::lock_guard<std::mutex> lock(threads_mutex_);

        threads_.emplace_back(
            [this, id]() {
                request_headers(id);
                peer_loop(id);
            });
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

                handle_frame(id, frame);

            } catch (...) {
                break;
            }
        }

        if (server_.peers().contains(id))
            server_.peers().remove_peer(id);
    }

    void handle_frame(std::uint64_t id, const P2PFrame& frame) {

        switch (frame.type) {

            case P2PMessageType::Blocks:
                handle_blocks(frame.payload);
                break;

            case P2PMessageType::GetBlocks:
                handle_get_blocks(id, frame.payload);
                break;

            case P2PMessageType::GetHeaders:
                handle_get_headers(id, frame.payload);
                break;

            case P2PMessageType::Headers:
                handle_headers(id, frame.payload);
                break;

            default:
                break;
        }
    }

    static P2PFrame make_get_blocks_frame(
        const std::vector<Hash256>& block_hashes) {

        if (block_hashes.empty() || block_hashes.size() > 1024)
            throw std::runtime_error(
                "invalid block request count");

        BinaryWriter writer;

        writer.write_u32(
            static_cast<std::uint32_t>(block_hashes.size()));

        for (const auto& hash : block_hashes)
            for (const auto byte : hash)
                writer.write_u8(byte);

        P2PFrame frame;
        frame.type = P2PMessageType::GetBlocks;
        frame.payload = writer.data();
        return frame;
    }

    void request_blocks(
        std::uint64_t id,
        const std::vector<Hash256>& block_hashes) {

        if (block_hashes.empty())
            return;

        auto connection =
            server_.peers().connection(id);

        if (!connection)
            return;

        constexpr std::size_t max_batch = 1024;

        for (std::size_t offset = 0;
             offset < block_hashes.size();
             offset += max_batch) {

            const std::size_t end =
                std::min(
                    offset + max_batch,
                    block_hashes.size());

            const std::vector<Hash256> batch(
                block_hashes.begin() + offset,
                block_hashes.begin() + end);

            connection->send_frame(
                make_get_blocks_frame(batch));
        }
    }

    void handle_headers(
        std::uint64_t id,
        const std::vector<std::uint8_t>& payload) {

        BinaryReader reader(payload);

        const std::uint32_t count =
            reader.read_u32();

        if (count == 0 || count > 2000)
            throw std::runtime_error(
                "invalid received header count");

        std::vector<BlockHeader> headers;
        headers.reserve(count);

        for (std::uint32_t i = 0; i < count; ++i) {

            const std::uint32_t size =
                reader.read_u32();

            if (size != 84)
                throw std::runtime_error(
                    "invalid block header size");

            const auto data =
                reader.read_bytes(size);

            const BlockHeader header =
                BlockHeader::deserialize_binary(data);

            if (header.version == 0)
                throw std::runtime_error(
                    "invalid received header version");

            if (header.difficulty > 256)
                throw std::runtime_error(
                    "invalid received header difficulty");

            BlockHeader pow_header = header;
            pow_header.nonce = 0;

            if (!validate_pow(
                    pow_header.serialize_binary(),
                    header.nonce,
                    header.difficulty)) {
                throw std::runtime_error(
                    "invalid received header proof of work");
            }

            headers.push_back(header);
        }

        if (!reader.empty())
            throw std::runtime_error(
                "trailing bytes in headers payload");

        /*
         * Headers are only useful if they form a direct extension
         * of our current chain.  We deliberately do not append
         * headers to storage: full blocks must arrive first.
         */
        if (!storage_.exists())
            throw std::runtime_error(
                "cannot process headers without local chain");

        std::vector<Hash256> block_hashes;
        block_hashes.reserve(headers.size());

        {
            std::lock_guard<std::mutex> lock(storage_mutex_);

            const auto chain = storage_.load();

            if (chain.empty())
                throw std::runtime_error(
                    "cannot process headers on empty chain");

            const Block& tip = chain.back();

            Hash256 previous_hash = tip.hash();
            std::uint64_t expected_height =
                tip.header.height + 1;

            for (const auto& header : headers) {

                if (header.height != expected_height)
                    throw std::runtime_error(
                        "received header height is not sequential");

                if (header.previous_hash != previous_hash)
                    throw std::runtime_error(
                        "received header does not extend local tip");

                block_hashes.push_back(header.hash());

                previous_hash = header.hash();
                ++expected_height;
            }
        }

        /*
         * We have now authenticated the header chain and know
         * exactly which full blocks are missing.  Request them
         * explicitly by hash.
         */
        request_blocks(id, block_hashes);
    }

    void handle_get_headers(
        std::uint64_t id,
        const std::vector<std::uint8_t>& payload) {

        BinaryReader reader(payload);

        const std::uint32_t count =
            reader.read_u32();

        if (count == 0 || count > 32)
            throw std::runtime_error(
                "invalid header locator count");

        std::vector<Hash256> locators;
        locators.reserve(count);

        for (std::uint32_t i = 0; i < count; ++i) {

            Hash256 hash{};

            for (auto& byte : hash)
                byte = reader.read_u8();

            locators.push_back(hash);
        }

        if (!reader.empty())
            throw std::runtime_error(
                "trailing bytes in getheaders payload");

        if (!storage_.exists())
            return;

        std::lock_guard<std::mutex> lock(storage_mutex_);

        const auto chain = storage_.load();

        if (chain.empty())
            return;

        std::size_t start = 0;

        for (const auto& locator : locators) {

            for (std::size_t i = chain.size(); i-- > 0;) {

                if (chain[i].hash() == locator) {
                    start = i + 1;
                    break;
                }
            }

            if (start != 0)
                break;
        }

        constexpr std::size_t MAX_HEADERS = 2000;

        const std::size_t end =
            std::min(chain.size(), start + MAX_HEADERS);

        BinaryWriter writer;

        writer.write_u32(
            static_cast<std::uint32_t>(end - start));

        for (std::size_t i = start; i < end; ++i) {

            const auto header =
                chain[i].header.serialize_binary();

            writer.write_u32(
                static_cast<std::uint32_t>(header.size()));

            writer.write_bytes(header);
        }

        P2PFrame response;
        response.type = P2PMessageType::Headers;
        response.payload = writer.data();

        auto connection = server_.peers().connection(id);
        if (connection)
            connection->send_frame(response);
    }

    void handle_get_blocks(
        std::uint64_t id,
        const std::vector<std::uint8_t>& payload) {

        BinaryReader reader(payload);

        const std::uint32_t count =
            reader.read_u32();

        if (count == 0 || count > 1024)
            throw std::runtime_error(
                "invalid getblocks request count");

        std::vector<Hash256> requested;
        requested.reserve(count);

        for (std::uint32_t i = 0; i < count; ++i) {

            Hash256 hash{};

            for (auto& byte : hash)
                byte = reader.read_u8();

            requested.push_back(hash);
        }

        if (!reader.empty())
            throw std::runtime_error(
                "trailing bytes in getblocks payload");

        if (!storage_.exists())
            return;

        std::lock_guard<std::mutex> lock(storage_mutex_);

        const auto chain = storage_.load();

        if (chain.empty())
            return;

        BinaryWriter writer;

        /*
         * Resolve requested hashes against the local canonical chain.
         * Only known blocks are returned.
         */
        std::vector<const Block*> blocks;
        blocks.reserve(requested.size());

        for (const auto& requested_hash : requested) {

            for (const auto& block : chain) {

                if (block.hash() == requested_hash) {
                    blocks.push_back(&block);
                    break;
                }
            }
        }

        if (blocks.empty())
            return;

        writer.write_u32(
            static_cast<std::uint32_t>(blocks.size()));

        for (const Block* block : blocks) {

            const auto encoded =
                block->serialize_full_binary();

            if (encoded.empty() ||
                encoded.size() > CZR_P2P_MAX_PAYLOAD ||
                encoded.size() >
                    std::numeric_limits<std::uint32_t>::max()) {
                throw std::runtime_error(
                    "block too large for getblocks response");
            }

            writer.write_u32(
                static_cast<std::uint32_t>(
                    encoded.size()));

            writer.write_bytes(encoded);
        }

        P2PFrame response;
        response.type = P2PMessageType::Blocks;
        response.payload = writer.data();

        auto connection =
            server_.peers().connection(id);

        if (connection)
            connection->send_frame(response);
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
