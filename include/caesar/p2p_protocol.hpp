#pragma once

#include <cstdint>

namespace caesar {

enum class P2PMessageType : std::uint8_t {
    Hello = 1,
    Ping = 2,
    Pong = 3,
    GetHeaders = 4,
    Headers = 5,
    GetBlocks = 6,
    Blocks = 7,
    GetMempool = 8,
    Transaction = 9,
    GetTransaction = 10,
    Reject = 11,
    GetSyncBlocks = 12,
    SyncBlocks = 13,
    GetPeers = 14,
    Peers = 15
};

/*
 * Wire-format bounds for P2PMessageType.
 *
 * MIN is the first valid message type.
 * MAX is the value of the last valid message type, derived from the
 * enum itself so the final protocol value stays in sync
 * automatically.
 *
 * IMPORTANT: if you add a new P2PMessageType, keep it before the
 * final protocol value represented by P2P_MESSAGE_TYPE_MAX. The test
 * CaesarP2PMessageTypeRangeTest walks every enum value that is listed
 * in tests/p2p_message_type_range_test.cpp and fails if the highest
 * value does not match MAX.
 */
inline constexpr std::uint8_t P2P_MESSAGE_TYPE_MIN =
    static_cast<std::uint8_t>(P2PMessageType::Hello);

inline constexpr std::uint8_t P2P_MESSAGE_TYPE_MAX =
    static_cast<std::uint8_t>(P2PMessageType::Peers);

inline const char* p2p_message_name(P2PMessageType type) noexcept {
    switch (type) {
        case P2PMessageType::Hello:
            return "hello";
        case P2PMessageType::Ping:
            return "ping";
        case P2PMessageType::Pong:
            return "pong";
        case P2PMessageType::GetHeaders:
            return "getheaders";
        case P2PMessageType::Headers:
            return "headers";
        case P2PMessageType::GetBlocks:
            return "getblocks";
        case P2PMessageType::Blocks:
            return "blocks";
        case P2PMessageType::GetMempool:
            return "getmempool";
        case P2PMessageType::Transaction:
            return "transaction";
        case P2PMessageType::GetTransaction:
            return "gettransaction";
        case P2PMessageType::Reject:
            return "reject";
        case P2PMessageType::GetSyncBlocks:
            return "getsyncblocks";
        case P2PMessageType::SyncBlocks:
            return "syncblocks";
        case P2PMessageType::GetPeers:
            return "getpeers";
        case P2PMessageType::Peers:
            return "peers";
    }

    return "unknown";
}

} // namespace caesar
