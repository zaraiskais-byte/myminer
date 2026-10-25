// Copyright (c) 2026 zaraiskais-byte. All rights reserved.
#pragma once

#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <vector>

#include <caesar/p2p_connection.hpp>
#include <caesar/p2p_frame.hpp>
#include <caesar/p2p_hello.hpp>

namespace caesar {

inline P2PFrame make_hello_frame(const P2PHello& hello) {
    P2PFrame frame;
    frame.type = P2PMessageType::Hello;
    frame.payload = hello.serialize_binary();
    return frame;
}

inline P2PHello parse_hello_frame(const P2PFrame& frame) {
    if (frame.type != P2PMessageType::Hello)
        throw std::runtime_error("Expected P2P hello frame");

    return P2PHello::deserialize_binary(frame.payload);
}

inline bool validate_hello(const P2PHello& hello, std::uint32_t expected_network_id) noexcept {
    return hello.protocol_version == CZR_P2P_PROTOCOL_VERSION &&
           hello.network_id == expected_network_id;
}

/*
 * Bounded-time hello handshake.
 *
 * The receive side is bounded by an absolute deadline so that a
 * peer that opens a TCP connection and then dribbles bytes cannot
 * hold the single accept thread in a receive_all() call for an
 * unbounded amount of time. The default 5-second budget is more
 * than enough for a peer with ordinary latency to exchange a
 * single Hello frame (well under 200 bytes on the wire).
 *
 * A slow or silent peer will be rejected by P2PTimeout, at which
 * point the caller (accept_loop) closes the socket and moves on to
 * the next connection.
 */
inline void perform_hello_handshake(
    const P2PConnection& connection, const P2PHello& local_hello,
    std::uint32_t expected_network_id,
    std::chrono::milliseconds timeout = std::chrono::seconds(5)) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;

    connection.send_frame(make_hello_frame(local_hello));

    const P2PFrame remote_frame =
        connection.receive_frame_with_deadline(deadline);

    const P2PHello remote_hello = parse_hello_frame(remote_frame);

    if (!validate_hello(remote_hello, expected_network_id)) {
        throw std::runtime_error("P2P hello validation failed");
    }
}

} // namespace caesar
