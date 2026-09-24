#include <cassert>
#include <cstdint>
#include <stdexcept>
#include <vector>

#include <caesar/p2p_sync_protocol.hpp>

int main() {
    using namespace caesar;

    P2PSyncSessionId session{
        0x1122334455667788ULL,
        0x99aabbccddeeff00ULL
    };

    Hash256 hash1{};
    Hash256 hash2{};

    for (std::size_t i = 0; i < hash1.size(); ++i) {
        hash1[i] =
            static_cast<std::uint8_t>(i);

        hash2[i] =
            static_cast<std::uint8_t>(0xff - i);
    }

    {
        GetSyncBlocksMessage message;
        message.session_id = session;
        message.block_hashes = {hash1, hash2};

        const auto encoded =
            message.serialize_binary();

        const auto decoded =
            GetSyncBlocksMessage::deserialize_binary(
                encoded);

        assert(decoded.session_id == session);
        assert(decoded.block_hashes.size() == 2);
        assert(decoded.block_hashes[0] == hash1);
        assert(decoded.block_hashes[1] == hash2);
    }

    {
        SyncBlocksMessage message;
        message.session_id = session;
        message.blocks = {
            {1, 2, 3, 4},
            {5, 6, 7, 8, 9}
        };

        const auto encoded =
            message.serialize_binary();

        const auto decoded =
            SyncBlocksMessage::deserialize_binary(
                encoded);

        assert(decoded.session_id == session);
        assert(decoded.blocks.size() == 2);
        assert(decoded.blocks[0] ==
               std::vector<std::uint8_t>(
                   {1, 2, 3, 4}));
        assert(decoded.blocks[1] ==
               std::vector<std::uint8_t>(
                   {5, 6, 7, 8, 9}));
    }

    {
        GetSyncBlocksMessage message;
        message.session_id = session;
        message.block_hashes = {hash1};

        auto encoded = message.serialize_binary();
        encoded.pop_back();

        bool rejected = false;

        try {
            (void)GetSyncBlocksMessage::
                deserialize_binary(encoded);
        } catch (const std::runtime_error&) {
            rejected = true;
        }

        assert(rejected);
    }

    {
        GetSyncBlocksMessage message;
        message.session_id = session;

        bool rejected = false;

        try {
            (void)message.serialize_binary();
        } catch (const std::runtime_error&) {
            rejected = true;
        }

        assert(rejected);
    }

    {
        SyncBlocksMessage message;
        message.session_id = session;
        message.blocks = {{1, 2, 3}};

        auto encoded = message.serialize_binary();
        encoded.push_back(0xaa);

        bool rejected = false;

        try {
            (void)SyncBlocksMessage::
                deserialize_binary(encoded);
        } catch (const std::runtime_error&) {
            rejected = true;
        }

        assert(rejected);
    }

    {
        BinaryWriter writer;

        writer.write_u64(session.high);
        writer.write_u64(session.low);
        writer.write_u32(0);

        bool rejected = false;

        try {
            (void)GetSyncBlocksMessage::
                deserialize_binary(writer.data());
        } catch (const std::runtime_error&) {
            rejected = true;
        }

        assert(rejected);
    }

    return 0;
}
