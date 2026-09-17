#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include <caesar/serialization.hpp>

namespace caesar {

constexpr std::uint32_t CZR_P2P_PROTOCOL_VERSION = 1;
constexpr std::uint32_t CZR_P2P_MAX_USER_AGENT = 128;

struct P2PHello {
    std::uint32_t protocol_version{CZR_P2P_PROTOCOL_VERSION};
    std::uint32_t network_id{1};
    std::uint64_t height{0};
    std::uint64_t timestamp{0};
    std::string user_agent{"Caesar-CZR"};

    std::vector<std::uint8_t> serialize_binary() const {
        if (user_agent.size() > CZR_P2P_MAX_USER_AGENT)
            throw std::runtime_error("P2P user agent too large");

        BinaryWriter writer;
        writer.write_u32(protocol_version);
        writer.write_u32(network_id);
        writer.write_u64(height);
        writer.write_u64(timestamp);
        writer.write_string(user_agent);

        return writer.data();
    }

    static P2PHello deserialize_binary(
        const std::vector<std::uint8_t>& data) {

        BinaryReader reader(data);

        P2PHello hello;
        hello.protocol_version = reader.read_u32();
        hello.network_id = reader.read_u32();
        hello.height = reader.read_u64();
        hello.timestamp = reader.read_u64();
        hello.user_agent = reader.read_string();

        if (hello.user_agent.size() > CZR_P2P_MAX_USER_AGENT)
            throw std::runtime_error("P2P user agent too large");

        if (reader.remaining() != 0)
            throw std::runtime_error("Trailing P2P hello data");

        return hello;
    }
};

} // namespace caesar
