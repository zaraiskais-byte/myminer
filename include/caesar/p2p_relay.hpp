#pragma once

#include <atomic>
#include <cstdint>
#include <limits>
#include <iostream>
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

        server_.peers().close_all_connections();

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

    static P2PFrame make_get_blocks_frame(
        const std::vector<std::uint64_t>& heights) {

        if (heights.empty() || heights.size() > 1024)
            throw std::runtime_error(
                "invalid block height count");

        BinaryWriter writer;
        writer.write_u32(
            static_cast<std::uint32_t>(heights.size()));

        for (const auto height : heights)
            writer.write_u64(height);

        P2PFrame frame;
        frame.type = P2PMessageType::GetBlocks;
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
        locators.reserve(32);

        /*
         * Build a Bitcoin-style locator sequence:
         * tip, then exponentially older blocks, and finally genesis.
         *
         * This lets a peer on a competing fork find the latest
         * common ancestor instead of requiring our current tip
         * to exist on its chain.
         */
        std::size_t index = chain.size() - 1;
        std::size_t step = 1;

        while (true) {
            locators.push_back(
                chain[index].hash());

            if (index == 0 ||
                locators.size() >= 31)
                break;

            if (index < step)
                index = 0;
            else
                index -= step;

            if (step <=
                std::numeric_limits<std::size_t>::max() / 2)
                step *= 2;
            else
                step = std::numeric_limits<std::size_t>::max();
        }

        if (locators.back() != chain.front().hash())
            locators.push_back(chain.front().hash());

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

            } catch (const std::exception& e) {
                std::cerr
                    << "[P2P] peer " << id
                    << " loop error: " << e.what()
                    << std::endl;
                break;
            } catch (...) {
                std::cerr
                    << "[P2P] peer " << id
                    << " loop error: unknown exception"
                    << std::endl;
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

            case P2PMessageType::GetHeaders:
                handle_get_headers(id, frame.payload);
                break;

            case P2PMessageType::Headers:
                handle_headers(id, frame.payload);
                break;

            case P2PMessageType::GetBlocks:
                handle_get_blocks(id, frame.payload);
                break;

            default:
                break;
        }
    }

    void handle_headers(
        std::uint64_t id,
        const std::vector<std::uint8_t>& payload) {

        BinaryReader reader(payload);

        const std::uint32_t count =
            reader.read_u32();

        if (count > 2000)
            throw std::runtime_error(
                "too many received headers");

        std::vector<BlockHeader> headers;
        headers.reserve(count);

        for (std::uint32_t i = 0; i < count; ++i) {
            const std::uint32_t size =
                reader.read_u32();

            if (size != 128)
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
                    header.difficulty))
                throw std::runtime_error(
                    "invalid received header proof of work");

            if (!headers.empty()) {
                const auto& previous = headers.back();

                if (header.height !=
                    previous.height + 1)
                    throw std::runtime_error(
                        "received headers are not sequential");

                if (header.previous_hash !=
                    previous.hash())
                    throw std::runtime_error(
                        "received headers have invalid internal link");
            }

            headers.push_back(header);
        }

        if (!reader.empty())
            throw std::runtime_error(
                "trailing bytes in headers payload");

        if (headers.empty())
            return;

        std::vector<Block> local_chain;

        {
            std::lock_guard<std::mutex> lock(storage_mutex_);

            if (!storage_.exists())
                return;

            local_chain = storage_.load();

            if (local_chain.empty())
                return;
        }

        /*
         * Find the local block immediately preceding the received
         * branch.  This deliberately allows the received headers to
         * have heights that already exist locally: that is the normal
         * case for a competing fork.
         */
        std::size_t ancestor_index = 0;
        bool found_anchor = false;

        for (const auto& header : headers) {
            if (header.height == 0)
                continue;

            const auto previous_height =
                header.height - 1;

            if (previous_height >= local_chain.size())
                continue;

            if (local_chain[
                    static_cast<std::size_t>(
                        previous_height)].hash() ==
                header.previous_hash) {

                ancestor_index =
                    static_cast<std::size_t>(
                        previous_height);

                found_anchor = true;
                break;
            }
        }

        if (!found_anchor)
            return;

        std::vector<std::uint64_t> heights;
        heights.reserve(
            std::min<std::size_t>(
                headers.size(),
                1024));

        bool started = false;

        for (const auto& header : headers) {
            if (header.height == 0)
                continue;

            if (!started) {
                if (header.height !=
                    ancestor_index + 1)
                    continue;

                if (header.previous_hash !=
                    local_chain[ancestor_index].hash())
                    continue;

                started = true;
            }

            heights.push_back(header.height);

            if (heights.size() >= 1024)
                break;
        }

        if (heights.empty())
            return;

        auto connection =
            server_.peers().connection(id);

        if (!connection)
            return;

        connection->send_frame(
            make_get_blocks_frame(heights));
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
                "invalid getblocks height count");

        std::vector<std::uint64_t> heights;
        heights.reserve(count);

        for (std::uint32_t i = 0; i < count; ++i)
            heights.push_back(reader.read_u64());

        if (!reader.empty())
            throw std::runtime_error(
                "trailing bytes in getblocks payload");

        if (!storage_.exists())
            return;

        std::lock_guard<std::mutex> lock(storage_mutex_);

        const auto chain = storage_.load();

        auto connection = server_.peers().connection(id);
        if (!connection)
            return;

        BinaryWriter writer;
        std::uint32_t found = 0;

        writer.write_u32(0);

        for (const auto requested_height : heights) {
            if (requested_height >= chain.size())
                continue;

            const auto& block =
                chain[static_cast<std::size_t>(
                    requested_height)];

            const auto encoded =
                block.serialize_full_binary();

            if (encoded.size() >
                static_cast<std::size_t>(
                    std::numeric_limits<std::uint32_t>::max()))
                throw std::runtime_error(
                    "serialized block too large");

            writer.write_u32(
                static_cast<std::uint32_t>(
                    encoded.size()));

            writer.write_bytes(encoded);
            ++found;
        }

        const auto bytes = writer.data();

        if (bytes.size() < sizeof(std::uint32_t))
            throw std::runtime_error(
                "invalid blocks response");

        std::vector<std::uint8_t> response_bytes = bytes;

        response_bytes[0] =
            static_cast<std::uint8_t>(found & 0xffU);
        response_bytes[1] =
            static_cast<std::uint8_t>((found >> 8) & 0xffU);
        response_bytes[2] =
            static_cast<std::uint8_t>((found >> 16) & 0xffU);
        response_bytes[3] =
            static_cast<std::uint8_t>((found >> 24) & 0xffU);

        P2PFrame response;
        response.type = P2PMessageType::Blocks;
        response.payload = std::move(response_bytes);

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

        std::vector<Block> received;
        received.reserve(count);

        for (std::uint32_t i = 0; i < count; ++i) {

            const std::uint32_t size =
                reader.read_u32();

            if (size > CZR_P2P_MAX_PAYLOAD)
                throw std::runtime_error(
                    "relayed block too large");

            const auto data =
                reader.read_bytes(size);

            received.push_back(
                Block::deserialize_full(data));
        }

        if (!reader.empty())
            throw std::runtime_error(
                "trailing bytes in relayed blocks payload");

        std::lock_guard<std::mutex> lock(
            storage_mutex_);

        if (!storage_.exists())
            throw std::runtime_error(
                "cannot process blocks without local chain");

        const auto local_chain =
            storage_.load();

        if (local_chain.empty())
            throw std::runtime_error(
                "local blockchain is empty");

        /*
         * The sender returns blocks in height order.
         * Sort defensively so the candidate construction does not
         * depend on transport ordering.
         */
        std::sort(
            received.begin(),
            received.end(),
            [](const Block& a, const Block& b) {
                return a.header.height <
                       b.header.height;
            });

        const Block& first =
            received.front();

        if (first.header.height == 0)
            throw std::runtime_error(
                "received genesis block is not processable");

        /*
         * Locate the local ancestor immediately preceding
         * the received branch.
         */
        std::size_t ancestor_index = 0;
        bool found_ancestor = false;

        for (std::size_t i = 0;
             i < local_chain.size();
             ++i) {

            if (local_chain[i].hash() ==
                first.header.previous_hash) {

                ancestor_index = i;
                found_ancestor = true;
                break;
            }
        }

        if (!found_ancestor)
            throw std::runtime_error(
                "received block does not connect to local chain");

        std::vector<Block> candidate;
        candidate.reserve(
            ancestor_index + 1 +
            received.size());

        candidate.insert(
            candidate.end(),
            local_chain.begin(),
            local_chain.begin() +
                static_cast<std::ptrdiff_t>(
                    ancestor_index + 1));

        for (const auto& block : received) {

            const auto expected_height =
                candidate.back().header.height + 1;

            if (block.header.height !=
                expected_height)
                throw std::runtime_error(
                    "received blocks are not sequential");

            if (block.header.previous_hash !=
                candidate.back().hash())
                throw std::runtime_error(
                    "received block has invalid previous hash");

            const auto previous_utxos =
                rebuild_utxo_set(candidate);

            if (!validate_block_consensus(
                    block,
                    candidate,
                    previous_utxos)) {

                std::cerr
                    << "[P2P] candidate CONSENSUS_FAIL"
                    << " height="
                    << block.header.height
                    << " previous="
                    << hash_to_hex(
                        block.header.previous_hash)
                    << std::endl;

                throw std::runtime_error(
                    "received block failed candidate consensus");
            }

            candidate.push_back(block);
        }

        const auto candidate_work =
            calculate_chain_work(candidate);

        const auto local_work =
            calculate_chain_work(local_chain);

        if (candidate_work <= local_work) {

            std::cerr
                << "[P2P] candidate rejected by chain-work rule"
                << " local_height="
                << local_chain.back().header.height
                << " candidate_height="
                << candidate.back().header.height
                << std::endl;

            return;
        }

        if (!storage_.replace_chain(candidate)) {

            std::cerr
                << "[P2P] replace_chain rejected candidate"
                << std::endl;

            return;
        }

        const bool was_reorg =
            ancestor_index + 1 <
            local_chain.size();

        if (was_reorg) {

            std::cerr
                << "[P2P] REORG accepted"
                << " ancestor_height="
                << local_chain[
                    ancestor_index].header.height
                << " old_height="
                << local_chain.back().header.height
                << " new_height="
                << candidate.back().header.height
                << std::endl;

        } else {

            std::cerr
                << "[P2P] chain extended"
                << " height="
                << candidate.back().header.height
                << std::endl;
        }

        /*
         * Relay only blocks that are now part of the canonical
         * chain.  A losing candidate is never broadcast.
         */
        for (const auto& block : received)
            server_.peers().broadcast(
                make_blocks_frame(block));
    }

    P2PServer& server_;
    BlockchainStorage& storage_;

    std::atomic<bool> running_{false};

    std::mutex threads_mutex_;
    std::vector<std::thread> threads_;

    std::mutex storage_mutex_;
};

} // namespace caesar
