#include <cassert>
#include <cstdint>
#include <stdexcept>
#include <vector>

#include <caesar/p2p_frame.hpp>

int main() {
    using namespace caesar;

    P2PFrame frame;
    frame.type = P2PMessageType::Transaction;
    frame.payload = {1, 2, 3, 4, 5};

    const auto encoded = frame.serialize_binary();
    const auto decoded = P2PFrame::deserialize_binary(encoded);

    assert(decoded.type == P2PMessageType::Transaction);
    assert(decoded.payload == frame.payload);

    {
        auto bad = encoded;
        bad[4] = 0;
        bad[5] = 255;

        bool rejected = false;

        try {
            (void)P2PFrame::deserialize_binary(bad);
        } catch (const std::runtime_error&) {
            rejected = true;
        }

        assert(rejected);
    }

    {
        auto bad = encoded;
        bad[0] = 6;

        bool rejected = false;

        try {
            (void)P2PFrame::deserialize_binary(bad);
        } catch (const std::runtime_error&) {
            rejected = true;
        }

        assert(rejected);
    }

    {
        P2PFrame oversized;
        oversized.type = P2PMessageType::Blocks;
        oversized.payload.resize(
            static_cast<std::size_t>(CZR_P2P_MAX_PAYLOAD) + 1);

        bool rejected = false;

        try {
            (void)oversized.serialize_binary();
        } catch (const std::runtime_error&) {
            rejected = true;
        }

        assert(rejected);
    }

    return 0;
}
