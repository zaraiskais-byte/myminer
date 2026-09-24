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
    SyncBlocks = 13
};

inline const char* p2p_message_name(
    P2PMessageType type) noexcept {

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
    }

    return "unknown";
}

} // namespace caesar
