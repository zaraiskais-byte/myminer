#pragma once

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <functional>
#include <limits>
#include <mutex>
#include <random>
#include <stdexcept>
#include <thread>
#include <unordered_map>
#include <vector>

#include <caesar/block.hpp>
#include <caesar/blockchain_storage.hpp>
#include <caesar/p2p_connection.hpp>
#include <caesar/p2p_frame.hpp>
#include <caesar/p2p_peer_manager.hpp>
#include <caesar/p2p_protocol.hpp>
#include <caesar/chain_replacement.hpp>
#include <caesar/p2p_sync_protocol.hpp>
#include <caesar/p2p_server.hpp>
#include <caesar/serialization.hpp>
#include <caesar/transaction.hpp>

namespace caesar {

// Wire format for Blocks payload:
//   [u32 count]
//   count * [u32 block_size][block_size bytes (Block::serialize_full_binary)]
class P2PRelay {
public:
    using ChainReplacementCallback =
        std::function<bool(const std::vector<Block>&)>;

    using TransactionCallback =
        std::function<bool(const Transaction&)>;

    P2PRelay(
        P2PServer& server,
        BlockchainStorage& storage)
        : server_(server),
          storage_(storage) {}

    void set_chain_replacement_callback(
        ChainReplacementCallback callback) {
        std::lock_guard<std::mutex> lock(pending_mutex_);
        chain_replacement_callback_ = std::move(callback);
    }

    void set_transaction_callback(
        TransactionCallback callback) {
        std::lock_guard<std::mutex> lock(transaction_callback_mutex_);
        transaction_callback_ = std::move(callback);
    }

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

        {
            std::lock_guard<std::mutex> lock(pending_mutex_);
            pending_chain_syncs_.clear();
        }
    }

    void announce_block(const Block& block) {
        server_.peers().broadcast(
            make_blocks_frame(block));
    }

    void announce_transaction(const Transaction& tx) {
        server_.peers().broadcast(
            make_transaction_frame(tx));
    }

private:
    struct PendingChainSync {
        P2PSyncSessionId session_id;
        std::vector<BlockHeader> headers;
        std::vector<Block> blocks;
        std::vector<bool> received;
    };

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

    static P2PFrame make_transaction_frame(
        const Transaction& tx) {

        const auto encoded =
            tx.serialize_full_binary();

        if (encoded.empty() ||
            encoded.size() > CZR_P2P_MAX_PAYLOAD ||
            encoded.size() >
                std::numeric_limits<std::uint32_t>::max()) {
            throw std::runtime_error(
                "transaction too large for P2P relay");
        }

        P2PFrame frame;
        frame.type = P2PMessageType::Transaction;
        frame.payload = encoded;
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

    static P2PSyncSessionId make_sync_session_id() {
        std::random_device rd;

        const std::uint64_t high =
            (static_cast<std::uint64_t>(rd()) << 32) |
            static_cast<std::uint64_t>(rd());

        const std::uint64_t low =
            (static_cast<std::uint64_t>(rd()) << 32) |
            static_cast<std::uint64_t>(rd());

        P2PSyncSessionId session{high, low};

        if (session.high == 0 && session.low == 0)
            session.low = 1;

        return session;
    }

    void request_sync_blocks(
        std::uint64_t id,
        const P2PSyncSessionId& session_id,
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

            GetSyncBlocksMessage message;
            message.session_id = session_id;
            message.block_hashes.assign(
                block_hashes.begin() + offset,
                block_hashes.begin() + end);

            P2PFrame frame;
            frame.type =
                P2PMessageType::GetSyncBlocks;
            frame.payload =
                message.serialize_binary();

            connection->send_frame(frame);
        }
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

        std::size_t index = chain.size() - 1;
        std::size_t step = 1;

        while (true) {
            locators.push_back(chain[index].hash());

            if (index == 0 || locators.size() == 32)
                break;

            const std::size_t next_index =
                (index > step) ? index - step : 0;

            index = next_index;

            if (locators.size() >= 10 &&
                step <= (std::numeric_limits<std::size_t>::max() / 2))
                step *= 2;
        }

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

        {
            std::lock_guard<std::mutex> lock(pending_mutex_);
            pending_chain_syncs_.erase(id);
        }

        if (server_.peers().contains(id))
            server_.peers().remove_peer(id);
    }

    void handle_frame(std::uint64_t id, const P2PFrame& frame) {

        switch (frame.type) {

            case P2PMessageType::Blocks:
                handle_blocks(id, frame.payload);
                break;

            case P2PMessageType::GetSyncBlocks:
                handle_get_sync_blocks(id, frame.payload);
                break;

            case P2PMessageType::SyncBlocks:
                handle_sync_blocks(id, frame.payload);
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

            case P2PMessageType::Transaction:
                handle_transaction(id, frame.payload);
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
         * Headers may either extend our current tip directly or
         * describe a competing fork rooted at a known local block.
         *
         * Headers are authenticated here, but full consensus validation
         * remains deferred until the complete blocks are assembled.
         */
        if (!storage_.exists())
            throw std::runtime_error(
                "cannot process headers without local chain");

        std::vector<Hash256> block_hashes;
        block_hashes.reserve(headers.size());

        bool direct_extension = false;

        {
            std::lock_guard<std::mutex> lock(storage_mutex_);

            const auto chain = storage_.load();

            if (chain.empty())
                throw std::runtime_error(
                    "cannot process headers on empty chain");

            const Block& tip = chain.back();

            direct_extension =
                headers.front().previous_hash == tip.hash();

            if (direct_extension) {
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
            } else {
                bool common_ancestor_found = false;

                for (const auto& local_block : chain) {
                    if (local_block.hash() ==
                        headers.front().previous_hash) {
                        common_ancestor_found = true;
                        break;
                    }
                }

                if (!common_ancestor_found)
                    throw std::runtime_error(
                        "received fork has no known common ancestor");

                Hash256 previous_hash =
                    headers.front().previous_hash;

                std::uint64_t expected_height =
                    headers.front().height;

                for (const auto& header : headers) {
                    if (header.height != expected_height)
                        throw std::runtime_error(
                            "received fork header height is not sequential");

                    if (header.previous_hash != previous_hash)
                        throw std::runtime_error(
                            "received fork header does not link sequentially");

                    block_hashes.push_back(header.hash());
                    previous_hash = header.hash();
                    ++expected_height;
                }
            }
        }

        if (!direct_extension) {
            const P2PSyncSessionId session_id =
                make_sync_session_id();

            {
                std::lock_guard<std::mutex> lock(
                    pending_mutex_);

                PendingChainSync pending;
                pending.session_id = session_id;
                pending.headers = headers;
                pending.blocks.resize(headers.size());
                pending.received.assign(
                    headers.size(),
                    false);

                pending_chain_syncs_[id] =
                    std::move(pending);
            }

            /*
             * Fork synchronization uses a session-aware wire path.
             * A later Headers response replaces the pending session;
             * responses carrying the old session id can therefore
             * never be attached to the new session.
             */
            request_sync_blocks(
                id,
                session_id,
                block_hashes);

            return;
        }

        /*
         * Direct extension keeps the existing GetBlocks/Blocks
         * path. This preserves ordinary block relay semantics.
         */
        request_blocks(id, block_hashes);
    }

    void handle_transaction(
        std::uint64_t id,
        const std::vector<std::uint8_t>& payload) {

        (void)id;

        /*
         * Transaction relay is deliberately defensive:
         * malformed or oversized transactions are ignored locally
         * rather than being allowed to tear down the peer loop.
         */
        if (payload.empty() ||
            payload.size() > CZR_P2P_MAX_PAYLOAD) {
            return;
        }

        try {
            const Transaction tx =
                Transaction::deserialize_full(payload);

            TransactionCallback callback;

            {
                std::lock_guard<std::mutex> lock(
                    transaction_callback_mutex_);

                callback = transaction_callback_;
            }

            /*
             * The node callback performs the authoritative mempool
             * admission check. Accepted transactions are announced by
             * the node's single transaction-relay path after admission.
             */
            if (callback)
                (void)callback(tx);

        } catch (...) {
            /*
             * Invalid transaction data is not propagated.
             * The connection remains usable for subsequent messages.
             */
        }
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

    void handle_get_sync_blocks(
        std::uint64_t id,
        const std::vector<std::uint8_t>& payload) {

        const GetSyncBlocksMessage request =
            GetSyncBlocksMessage::deserialize_binary(
                payload);

        if (!storage_.exists())
            throw std::runtime_error(
                "cannot serve sync blocks without local chain");

        const auto chain = storage_.load();

        std::vector<Block> requested;
        requested.reserve(
            request.block_hashes.size());

        for (const auto& hash :
             request.block_hashes) {

            const auto it =
                std::find_if(
                    chain.begin(),
                    chain.end(),
                    [&](const Block& block) {
                        return block.hash() == hash;
                    });

            if (it == chain.end())
                throw std::runtime_error(
                    "requested sync block is not on canonical chain");

            requested.push_back(*it);
        }

        auto connection =
            server_.peers().connection(id);

        if (!connection)
            return;

        SyncBlocksMessage response;
        response.session_id =
            request.session_id;

        for (const auto& block : requested) {
            const auto encoded =
                block.serialize_full_binary();

            if (encoded.empty() ||
                encoded.size() >
                    static_cast<std::size_t>(
                        CZR_P2P_MAX_PAYLOAD)) {
                throw std::runtime_error(
                    "sync block exceeds payload limit");
            }

            /*
             * Keep every SyncBlocks frame below the P2P frame
             * payload limit. A session may therefore receive
             * multiple responses with the same session id.
             */
            if (!response.blocks.empty()) {
                const std::size_t projected =
                    16 + 4 +
                    response.blocks.size() * 4;

                std::size_t total =
                    projected;

                for (const auto& existing :
                     response.blocks)
                    total += existing.size();

                total += 4 + encoded.size();

                if (total >
                    static_cast<std::size_t>(
                        CZR_P2P_MAX_PAYLOAD)) {

                    P2PFrame frame;
                    frame.type =
                        P2PMessageType::SyncBlocks;
                    frame.payload =
                        response.serialize_binary();

                    connection->send_frame(frame);

                    response.blocks.clear();
                }
            }

            response.blocks.push_back(encoded);
        }

        if (!response.blocks.empty()) {
            P2PFrame frame;
            frame.type =
                P2PMessageType::SyncBlocks;
            frame.payload =
                response.serialize_binary();

            connection->send_frame(frame);
        }
    }

    void handle_sync_blocks(
        std::uint64_t id,
        const std::vector<std::uint8_t>& payload) {

        const SyncBlocksMessage message =
            SyncBlocksMessage::deserialize_binary(
                payload);

        std::vector<BlockHeader> headers;
        std::vector<Block> blocks;
        bool complete = false;

        {
            std::lock_guard<std::mutex> lock(
                pending_mutex_);

            auto pending_it =
                pending_chain_syncs_.find(id);

            /*
             * A response for a completed/replaced session is stale.
             * It is deliberately ignored rather than being interpreted
             * as ordinary block relay.
             */
            if (pending_it ==
                pending_chain_syncs_.end()) {
                return;
            }

            auto& pending =
                pending_it->second;

            if (pending.session_id !=
                message.session_id) {
                return;
            }

            for (const auto& encoded :
                 message.blocks) {

                const Block block =
                    Block::deserialize_full(encoded);

                std::size_t index =
                    pending.headers.size();

                for (std::size_t i = 0;
                     i < pending.headers.size();
                     ++i) {

                    if (pending.headers[i].hash() ==
                        block.hash()) {
                        index = i;
                        break;
                    }
                }

                if (index == pending.headers.size())
                    throw std::runtime_error(
                        "sync block was not requested");

                if (pending.received[index])
                    throw std::runtime_error(
                        "duplicate sync block");

                if (block.header.hash() !=
                    pending.headers[index].hash())
                    throw std::runtime_error(
                        "sync block header mismatch");

                pending.blocks[index] = block;
                pending.received[index] = true;
            }

            complete =
                std::all_of(
                    pending.received.begin(),
                    pending.received.end(),
                    [](bool value) {
                        return value;
                    });

            if (complete) {
                headers = pending.headers;
                blocks = pending.blocks;
                pending_chain_syncs_.erase(
                    pending_it);
            }
        }

        if (!complete)
            return;

        std::vector<Block> current;

        {
            std::lock_guard<std::mutex> lock(
                storage_mutex_);

            if (!storage_.exists())
                throw std::runtime_error(
                    "cannot assemble sync candidate without local chain");

            current = storage_.load();
        }

        const auto candidate =
            assemble_candidate_chain(
                current,
                headers,
                blocks);

        if (!candidate)
            throw std::runtime_error(
                "failed to assemble sync candidate");

        ChainReplacementCallback callback;

        {
            std::lock_guard<std::mutex> lock(
                pending_mutex_);

            callback =
                chain_replacement_callback_;
        }

        if (callback && !callback(*candidate))
            throw std::runtime_error(
                "sync candidate was rejected");
    }

    void handle_blocks(
        std::uint64_t id,
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

    ChainReplacementCallback chain_replacement_callback_;
    mutable std::mutex transaction_callback_mutex_;
    TransactionCallback transaction_callback_;

    std::atomic<bool> running_{false};

    std::mutex threads_mutex_;
    std::vector<std::thread> threads_;

    std::mutex storage_mutex_;

    mutable std::mutex pending_mutex_;
    std::unordered_map<
        std::uint64_t,
        PendingChainSync> pending_chain_syncs_;
};

} // namespace caesar
