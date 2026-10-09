#pragma once

#include <arpa/inet.h>

#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <caesar/p2p_frame.hpp>
#include <caesar/p2p_protocol.hpp>
#include <caesar/serialization.hpp>

namespace caesar {

/*
 * Peer discovery is deliberately bounded.
 *
 * This protocol exchanges endpoints only. It does not exchange
 * wallet data, private keys, authentication material, or chain data.
 */
inline constexpr std::uint32_t CZR_P2P_MAX_DISCOVERY_PEERS = 32;
inline constexpr std::uint32_t CZR_P2P_MAX_PEER_ADDRESS = 255;

struct P2PPeerEndpoint {
    std::string address;
    std::uint16_t port{0};

    void validate() const {
        if (address.empty())
            throw std::runtime_error("empty P2P peer address");

        if (address.size() > CZR_P2P_MAX_PEER_ADDRESS)
            throw std::runtime_error("P2P peer address too long");

        if (port == 0)
            throw std::runtime_error("invalid P2P peer port");

        in_addr parsed{};
        if (::inet_pton(AF_INET, address.c_str(), &parsed) != 1)
            throw std::runtime_error("invalid P2P IPv4 peer address");

        const std::uint32_t host =
            ntohl(parsed.s_addr);

        const std::uint32_t first_octet =
            (host >> 24) & 0xffU;

        /*
         * Discovery candidates must be IPv4 unicast
         * endpoints. Private/LAN addresses remain valid;
         * only addresses that cannot identify a unicast
         * peer endpoint are rejected here.
         */
        if (first_octet == 0U)
            throw std::runtime_error(
                "unspecified P2P peer address");

        if (first_octet == 127U)
            throw std::runtime_error(
                "loopback P2P peer address");

        if (first_octet >= 224U)
            throw std::runtime_error(
                "non-unicast P2P peer address");
    }
};

inline P2PFrame make_get_peers_frame() {
    P2PFrame frame;
    frame.type = P2PMessageType::GetPeers;
    return frame;
}

inline std::vector<P2PPeerEndpoint>
parse_get_peers_payload(const std::vector<std::uint8_t>& payload) {
    if (!payload.empty())
        throw std::runtime_error("GetPeers payload must be empty");

    return {};
}

inline P2PFrame make_peers_frame(
    const std::vector<P2PPeerEndpoint>& peers) {

    if (peers.size() > CZR_P2P_MAX_DISCOVERY_PEERS)
        throw std::runtime_error("too many discovery peers");

    BinaryWriter writer;
    writer.write_u32(static_cast<std::uint32_t>(peers.size()));

    for (const auto& peer : peers) {
        peer.validate();

        writer.write_string(peer.address);
        writer.write_u32(static_cast<std::uint32_t>(peer.port));
    }

    P2PFrame frame;
    frame.type = P2PMessageType::Peers;
    frame.payload = writer.data();
    return frame;
}

inline std::vector<P2PPeerEndpoint>
parse_peers_payload(const std::vector<std::uint8_t>& payload) {

    BinaryReader reader(payload);

    const std::uint32_t count = reader.read_u32();

    if (count > CZR_P2P_MAX_DISCOVERY_PEERS)
        throw std::runtime_error("too many discovery peers");

    std::vector<P2PPeerEndpoint> peers;
    peers.reserve(count);

    for (std::uint32_t i = 0; i < count; ++i) {
        const std::string address = reader.read_string();

        if (address.empty())
            throw std::runtime_error("empty discovered peer address");

        if (address.size() > CZR_P2P_MAX_PEER_ADDRESS)
            throw std::runtime_error("discovered peer address too long");

        const std::uint32_t raw_port = reader.read_u32();

        if (raw_port == 0 || raw_port > 65535)
            throw std::runtime_error("invalid discovered peer port");

        peers.push_back(P2PPeerEndpoint{
            address,
            static_cast<std::uint16_t>(raw_port)
        });
    }

    if (reader.remaining() != 0)
        throw std::runtime_error("trailing discovery payload");

    return peers;
}

} // namespace caesar
