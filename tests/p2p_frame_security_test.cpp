#include <cassert>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <vector>

#include <caesar/p2p_frame.hpp>

using namespace caesar;

static void expect_reject(
    const std::vector<std::uint8_t>& data) {

    bool rejected = false;

    try {
        (void)P2PFrame::deserialize_binary(data);
    } catch (const std::exception&) {
        rejected = true;
    }

    assert(rejected);
}

static std::vector<std::uint8_t> u32le(
    std::uint32_t value) {

    return {
        static_cast<std::uint8_t>(value & 0xff),
        static_cast<std::uint8_t>((value >> 8) & 0xff),
        static_cast<std::uint8_t>((value >> 16) & 0xff),
        static_cast<std::uint8_t>((value >> 24) & 0xff)
    };
}

int main() {
    // 1. Truncated frame header.
    expect_reject({0x01, 0x00, 0x00});

    // 2. Declared payload length exceeds protocol limit.
    {
        auto data = u32le(CZR_P2P_MAX_PAYLOAD + 1);
        data.push_back(
            static_cast<std::uint8_t>(
                P2PMessageType::Hello));
        expect_reject(data);
    }

    // 3. Invalid message type: zero.
    {
        auto data = u32le(0);
        data.push_back(0);
        expect_reject(data);
    }

    // 4. Invalid message type: above the known protocol range.
    {
        auto data = u32le(0);
        data.push_back(12);
        expect_reject(data);
    }

    // 5. Declared payload is larger than bytes actually supplied.
    {
        auto data = u32le(10);
        data.push_back(
            static_cast<std::uint8_t>(
                P2PMessageType::Hello));
        data.push_back(0xAA);
        expect_reject(data);
    }

    // 6. Declared payload is smaller than trailing bytes.
    {
        auto data = u32le(1);
        data.push_back(
            static_cast<std::uint8_t>(
                P2PMessageType::Hello));
        data.push_back(0xAA);
        data.push_back(0xBB);
        expect_reject(data);
    }

    // 7. A valid empty-payload frame must still decode.
    {
        P2PFrame frame;
        frame.type = P2PMessageType::Ping;
        frame.payload = {};

        const auto encoded = frame.serialize_binary();
        const auto decoded =
            P2PFrame::deserialize_binary(encoded);

        assert(decoded.type == P2PMessageType::Ping);
        assert(decoded.payload.empty());
    }

    // 8. Maximum permitted payload must be accepted.
    {
        P2PFrame frame;
        frame.type = P2PMessageType::Transaction;
        frame.payload.resize(CZR_P2P_MAX_PAYLOAD, 0x5A);

        const auto encoded = frame.serialize_binary();
        const auto decoded =
            P2PFrame::deserialize_binary(encoded);

        assert(decoded.type ==
               P2PMessageType::Transaction);
        assert(decoded.payload.size() ==
               CZR_P2P_MAX_PAYLOAD);
        assert(decoded.payload.front() == 0x5A);
        assert(decoded.payload.back() == 0x5A);
    }

    // 9. Serialization itself must reject oversized payloads.
    {
        P2PFrame frame;
        frame.type = P2PMessageType::Blocks;
        frame.payload.resize(
            static_cast<std::size_t>(
                CZR_P2P_MAX_PAYLOAD) + 1,
            0xA5);

        bool rejected = false;

        try {
            (void)frame.serialize_binary();
        } catch (const std::exception&) {
            rejected = true;
        }

        assert(rejected);
    }

    return 0;
}
