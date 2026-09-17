#include <cassert>
#include <chrono>
#include <cstdint>
#include <thread>

#include <caesar/p2p_connection.hpp>
#include <caesar/p2p_hello.hpp>
#include <caesar/p2p_handshake.hpp>
#include <caesar/p2p_server.hpp>

int main() {
    constexpr std::uint16_t port = 39425;
    constexpr std::uint32_t network_id = 1;

    caesar::P2PServer server;

    server.start(
        port,
        "127.0.0.1",
        network_id,
        100);

    assert(server.running());

    caesar::P2PConnection client;
    client.connect_to("127.0.0.1", port);

    caesar::P2PHello client_hello;
    client_hello.protocol_version =
        caesar::CZR_P2P_PROTOCOL_VERSION;
    client_hello.network_id = network_id;
    client_hello.height = 99;
    client_hello.timestamp = 123456;
    client_hello.user_agent = "Caesar-CZR-Test";

    client.send_frame(
        caesar::make_hello_frame(client_hello));

    caesar::P2PFrame server_frame =
        client.receive_frame();

    caesar::P2PHello server_hello =
        caesar::parse_hello_frame(server_frame);

    assert(server_hello.protocol_version ==
           caesar::CZR_P2P_PROTOCOL_VERSION);

    assert(server_hello.network_id == network_id);

    assert(server_hello.height == 100);

    assert(server_hello.user_agent == "Caesar-CZR");

    for (int i = 0; i < 100; ++i) {
        if (server.peer_count() == 1)
            break;

        std::this_thread::sleep_for(
            std::chrono::milliseconds(10));
    }

    assert(server.peer_count() == 1);

    client = caesar::P2PConnection{};

    server.stop();

    assert(!server.running());
    assert(server.peer_count() == 0);

    return 0;
}
