#pragma once

#include <cstdint>
#include <stdexcept>
#include <vector>

#include <caesar/p2p_protocol.hpp>
#include <caesar/serialization.hpp>

namespace caesar {

constexpr std::uint32_t CZR_P2P_MAX_PAYLOAD = 4 * 1024 * 1024;

struct P2PFrame {
    P2PMessageType type{P2PMessageType::Reject};
    std::vector<std::uint8_t> payload;

    std::vector<std::uint8_t> serialize_binary() const {
        if (payload.size() > CZR_P2P_MAX_PAYLOAD)
            throw std::runtime_error("P2P payload too large");

        BinaryWriter writer;
        writer.write_u32(static_cast<std::uint32_t>(payload.size()));
        writer.write_u8(static_cast<std::uint8_t>(type));
        writer.write_bytes(payload);

        return writer.data();
    }

    static P2PFrame deserialize_binary(
        const std::vector<std::uint8_t>& data) {

        BinaryReader reader(data);

        const std::uint32_t payload_size =
            reader.read_u32();

        if (payload_size > CZR_P2P_MAX_PAYLOAD)
            throw std::runtime_error("P2P payload too large");

        const auto raw_type = reader.read_u8();

        if (raw_type == 0 || raw_type > 11)
            throw std::runtime_error("Invalid P2P message type");

        if (reader.remaining() != payload_size)
            throw std::runtime_error("Invalid P2P frame length");

        P2PFrame frame;
        frame.type =
            static_cast<P2PMessageType>(raw_type);
        frame.payload =
            reader.read_bytes(payload_size);

        return frame;
    }
};

} // namespace caesar
