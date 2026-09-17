#pragma once

#include <cstdint>
#include <stdexcept>
#include <vector>

#include <caesar/p2p_frame.hpp>
#include <caesar/p2p_connection.hpp>
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
        throw std::runtime_error(
            "Expected P2P hello frame");

    return P2PHello::deserialize_binary(
        frame.payload);
}

inline bool validate_hello(
    const P2PHello& hello,
    std::uint32_t expected_network_id) noexcept {

    return
        hello.protocol_version ==
            CZR_P2P_PROTOCOL_VERSION &&
        hello.network_id ==
            expected_network_id;
}

inline void perform_hello_handshake(
    const P2PConnection& connection,
    const P2PHello& local_hello,
    std::uint32_t expected_network_id) {

    connection.send_frame(
        make_hello_frame(local_hello));

    const P2PFrame remote_frame =
        connection.receive_frame();

    const P2PHello remote_hello =
        parse_hello_frame(remote_frame);

    if (!validate_hello(
            remote_hello,
            expected_network_id)) {

        throw std::runtime_error(
            "P2P hello validation failed");
    }
}

}
