/*
 * Regression test for the P2P message type wire bound.
 *
 * p2p_frame.hpp used to reject frames whose raw type byte was > 13.
 * That number was a magic constant independent of P2PMessageType. If
 * a new type were added to the enum without updating the check, every
 * frame carrying the new type would be rejected at the wire boundary,
 * silently breaking the protocol.
 *
 * The fix introduced P2P_MESSAGE_TYPE_MIN and P2P_MESSAGE_TYPE_MAX in
 * p2p_protocol.hpp and made the frame parser use them.
 *
 * This test does two things:
 *   1. For every known P2PMessageType value, asserts it lies within
 *      [MIN, MAX].
 *   2. Computes the maximum value among known types and asserts it
 *      equals P2P_MESSAGE_TYPE_MAX. If a new type is added to the
 *      enum and to the list below but MAX is forgotten, this fails.
 *
 * It also feeds sample frames through P2PFrame::deserialize_binary()
 * to confirm that valid types still parse and out-of-range types are
 * still rejected.
 */

#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

#include <caesar/p2p_frame.hpp>
#include <caesar/p2p_protocol.hpp>

using namespace caesar;

namespace {

std::vector<std::uint8_t> make_frame_bytes(std::uint8_t raw_type,
                                           const std::vector<std::uint8_t>& payload) {
    BinaryWriter writer;
    writer.write_u32(static_cast<std::uint32_t>(payload.size()));
    writer.write_u8(raw_type);
    writer.write_bytes(payload);
    return writer.data();
}

} // namespace

int main() {
    // Every message type we ship must lie within [MIN, MAX].
    const P2PMessageType all_types[] = {
        P2PMessageType::Hello,          P2PMessageType::Ping,       P2PMessageType::Pong,
        P2PMessageType::GetHeaders,     P2PMessageType::Headers,    P2PMessageType::GetBlocks,
        P2PMessageType::Blocks,         P2PMessageType::GetMempool, P2PMessageType::Transaction,
        P2PMessageType::GetTransaction, P2PMessageType::Reject,     P2PMessageType::GetSyncBlocks,
        P2PMessageType::SyncBlocks,
    };

    std::uint8_t observed_max = 0;

    for (auto t : all_types) {
        const auto raw = static_cast<std::uint8_t>(t);
        assert(raw >= P2P_MESSAGE_TYPE_MIN);
        assert(raw <= P2P_MESSAGE_TYPE_MAX);
        if (raw > observed_max)
            observed_max = raw;
    }

    std::cout << "[range] MIN=" << static_cast<int>(P2P_MESSAGE_TYPE_MIN)
              << " MAX=" << static_cast<int>(P2P_MESSAGE_TYPE_MAX)
              << " observed_max=" << static_cast<int>(observed_max) << "\n";

    assert(observed_max == P2P_MESSAGE_TYPE_MAX &&
           "highest P2PMessageType value does not match "
           "P2P_MESSAGE_TYPE_MAX; update the bound constant "
           "and this test when adding a new type");

    // Valid types parse.
    for (auto t : all_types) {
        const auto raw = static_cast<std::uint8_t>(t);
        const auto bytes = make_frame_bytes(raw, {});
        const auto parsed = P2PFrame::deserialize_binary(bytes);
        assert(static_cast<std::uint8_t>(parsed.type) == raw);
    }

    // 0 and values above MAX are rejected.
    bool zero_rejected = false;
    try {
        (void)P2PFrame::deserialize_binary(make_frame_bytes(0, {}));
    } catch (const std::exception&) {
        zero_rejected = true;
    }
    assert(zero_rejected);

    bool above_max_rejected = false;
    try {
        (void)P2PFrame::deserialize_binary(
            make_frame_bytes(static_cast<std::uint8_t>(P2P_MESSAGE_TYPE_MAX + 1), {}));
    } catch (const std::exception&) {
        above_max_rejected = true;
    }
    assert(above_max_rejected);

    std::cout << "CaesarP2PMessageTypeRangeTest: PASS\n";
    return 0;
}
