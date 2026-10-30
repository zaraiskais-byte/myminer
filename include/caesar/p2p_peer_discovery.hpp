// Copyright (c) 2026 zaraiskais-byte. All rights reserved.
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

/*
 * Discovery endpoints are IPv4-only by design.
 *
 * The Peers wire message carries a length-prefixed address
 * string plus a u32 port. Only dotted-quad IPv4 addresses are
 * accepted. IPv6 support would require:
 *   - a family discriminator in the wire format (or a text
 *     format that unambiguously separates v4 from v6),
 *   - a separate size budget from CZR_P2P_MAX_PEER_ADDRESS,
 *   - a re-review of the loopback/multicast classification
 *     below, which assumes in_addr and exactly 4 octets.
 *
 * Until then, a peer that sends an IPv6 address is rejected
 * by validate() as "invalid P2P IPv4 peer address". This is
 * a deliberate scope limit, not an oversight. The wire
 * format is unchanged: the IPv4 restriction lives entirely
 * in validate().
 */
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

        /*
         * Loopback (127.0.0.0/8) is rejected even though a
         * peer on the same host is reachable through it.
         * Accepting loopback would let a remote peer inject
         * "127.0.0.1:<rpc_port>" into the discovery set and
         * turn the outbound dialer into a confused-deputy
         * gadget against local services.
         *
         * Private LAN ranges (10/8, 172.16/12, 192.168/16)
         * are intentionally NOT rejected here: they are the
         * normal case for a LAN-only deployment, and the
         * connect path re-checks reachability anyway.
         */
        if (first_octet == 127U)
            throw std::runtime_error(
                "loopback P2P peer address");

        /*
         * Multicast (224.0.0.0/4) and reserved (240.0.0.0/4,
         * which includes the 255.255.255.255 limited
         * broadcast) are not unicast endpoints. A discovery
         * list containing them would either silently fail to
         * connect (multicast requires a socket option the P2P
         * client does not set) or, for 255.255.255.255, be
         * usable to amplify by dialing every local interface.
         * Reject both at the trust boundary.
         */
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

        /*
         * parse_peers_payload is a trust boundary: the address
         * string arrives from an untrusted remote peer. The
         * length and port checks above are not sufficient.
         *
         * Without validate(), a hostile peer can inject:
         *   - non-IPv4 strings ("not-an-ip") that pass the
         *     length check but fail inet_pton;
         *   - loopback (127.0.0.0/8), turning the outbound
         *     dialer into a confused deputy against local
         *     services;
         *   - multicast / reserved / broadcast (>= 224.0.0.0),
         *     which are not unicast endpoints.
         *
         * make_peers_frame() already calls validate() on the
         * send path. This call makes the receive path
         * symmetric, so the two halves agree on what a valid
         * endpoint is.
         */
        P2PPeerEndpoint peer{
            address,
            static_cast<std::uint16_t>(raw_port)
        };
        peer.validate();
        peers.push_back(std::move(peer));
    }

    if (reader.remaining() != 0)
        throw std::runtime_error("trailing discovery payload");

    return peers;
}

} // namespace caesar
