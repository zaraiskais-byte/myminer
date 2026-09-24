#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

#include <caesar/crypto.hpp>
#include <caesar/p2p_frame.hpp>
#include <caesar/p2p_protocol.hpp>
#include <caesar/serialization.hpp>

namespace caesar {

struct P2PSyncSessionId {
    std::uint64_t high{0};
    std::uint64_t low{0};

    bool operator==(const P2PSyncSessionId& other) const noexcept {
        return high == other.high && low == other.low;
    }

    bool operator!=(const P2PSyncSessionId& other) const noexcept {
        return !(*this == other);
    }
};

inline void write_sync_session_id(
    BinaryWriter& writer,
    const P2PSyncSessionId& id) {
    writer.write_u64(id.high);
    writer.write_u64(id.low);
}

inline P2PSyncSessionId read_sync_session_id(
    BinaryReader& reader) {
    P2PSyncSessionId id;
    id.high = reader.read_u64();
    id.low = reader.read_u64();
    return id;
}

struct GetSyncBlocksMessage {
    static constexpr std::uint32_t MAX_HASHES = 1024;

    P2PSyncSessionId session_id;
    std::vector<Hash256> block_hashes;

    std::vector<std::uint8_t> serialize_binary() const {
        if (block_hashes.empty())
            throw std::runtime_error(
                "empty sync block request");

        if (block_hashes.size() > MAX_HASHES)
            throw std::runtime_error(
                "too many sync block hashes");

        BinaryWriter writer;
        write_sync_session_id(writer, session_id);

        writer.write_u32(
            static_cast<std::uint32_t>(
                block_hashes.size()));

        for (const auto& hash : block_hashes) {
            writer.write_bytes(
                std::vector<std::uint8_t>(
                    hash.begin(),
                    hash.end()));
        }

        return writer.data();
    }

    static GetSyncBlocksMessage deserialize_binary(
        const std::vector<std::uint8_t>& data) {
        BinaryReader reader(data);

        GetSyncBlocksMessage message;
        message.session_id =
            read_sync_session_id(reader);

        const std::uint32_t count =
            reader.read_u32();

        if (count == 0 || count > MAX_HASHES)
            throw std::runtime_error(
                "invalid sync block request count");

        message.block_hashes.reserve(count);

        for (std::uint32_t i = 0; i < count; ++i) {
            const auto bytes = reader.read_bytes(32);

            Hash256 hash{};
            std::copy(
                bytes.begin(),
                bytes.end(),
                hash.begin());

            message.block_hashes.push_back(hash);
        }

        if (!reader.empty())
            throw std::runtime_error(
                "trailing sync block request data");

        return message;
    }
};

struct SyncBlocksMessage {
    static constexpr std::uint32_t MAX_BLOCKS = 1024;

    P2PSyncSessionId session_id;
    std::vector<std::vector<std::uint8_t>> blocks;

    std::vector<std::uint8_t> serialize_binary() const {
        if (blocks.empty())
            throw std::runtime_error(
                "empty sync block response");

        if (blocks.size() > MAX_BLOCKS)
            throw std::runtime_error(
                "too many sync blocks");

        BinaryWriter writer;
        write_sync_session_id(writer, session_id);

        writer.write_u32(
            static_cast<std::uint32_t>(
                blocks.size()));

        for (const auto& block : blocks) {
            if (block.empty())
                throw std::runtime_error(
                    "empty serialized sync block");

            if (block.size() >
                static_cast<std::size_t>(
                    CZR_P2P_MAX_PAYLOAD)) {
                throw std::runtime_error(
                    "sync block too large");
            }

            writer.write_u32(
                static_cast<std::uint32_t>(
                    block.size()));

            writer.write_bytes(block);
        }

        return writer.data();
    }

    static SyncBlocksMessage deserialize_binary(
        const std::vector<std::uint8_t>& data) {
        BinaryReader reader(data);

        SyncBlocksMessage message;
        message.session_id =
            read_sync_session_id(reader);

        const std::uint32_t count =
            reader.read_u32();

        if (count == 0 || count > MAX_BLOCKS)
            throw std::runtime_error(
                "invalid sync block response count");

        message.blocks.reserve(count);

        for (std::uint32_t i = 0; i < count; ++i) {
            const std::uint32_t size =
                reader.read_u32();

            if (size == 0 ||
                size > CZR_P2P_MAX_PAYLOAD) {
                throw std::runtime_error(
                    "sync block too large");
            }

            message.blocks.push_back(
                reader.read_bytes(size));
        }

        if (!reader.empty())
            throw std::runtime_error(
                "trailing sync block response data");

        return message;
    }
};

} // namespace caesar
