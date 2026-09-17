#pragma once

#include <cstdint>
#include <stdexcept>
#include <vector>

#include <caesar/serialization.hpp>

namespace caesar {

struct P2PPing {
    std::uint64_t nonce{0};

    std::vector<std::uint8_t> serialize_binary() const {
        BinaryWriter writer;
        writer.write_u64(nonce);
        return writer.data();
    }

    static P2PPing deserialize_binary(
        const std::vector<std::uint8_t>& data) {

        BinaryReader reader(data);

        P2PPing ping;
        ping.nonce = reader.read_u64();

        if (reader.remaining() != 0)
            throw std::runtime_error("Trailing P2P ping data");

        return ping;
    }
};

struct P2PPong {
    std::uint64_t nonce{0};

    std::vector<std::uint8_t> serialize_binary() const {
        BinaryWriter writer;
        writer.write_u64(nonce);
        return writer.data();
    }

    static P2PPong deserialize_binary(
        const std::vector<std::uint8_t>& data) {

        BinaryReader reader(data);

        P2PPong pong;
        pong.nonce = reader.read_u64();

        if (reader.remaining() != 0)
            throw std::runtime_error("Trailing P2P pong data");

        return pong;
    }
};

}
