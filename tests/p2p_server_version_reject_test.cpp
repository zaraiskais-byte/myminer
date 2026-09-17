#include <cassert>
#include <chrono>
#include <cstdint>
#include <thread>

#include <caesar/p2p_connection.hpp>
#include <caesar/p2p_handshake.hpp>
#include <caesar/p2p_server.hpp>

int main() {
    constexpr std::uint16_t port = 39427;
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

    caesar::P2PHello bad_hello;
    bad_hello.protocol_version =
        caesar::CZR_P2P_PROTOCOL_VERSION + 1;
    bad_hello.network_id = network_id;
    bad_hello.height = 99;
    bad_hello.timestamp = 123456;
    bad_hello.user_agent = "Caesar-CZR-Bad-Version";

    client.send_frame(
        caesar::make_hello_frame(bad_hello));

    std::this_thread::sleep_for(
        std::chrono::milliseconds(200));

    assert(server.peer_count() == 0);

    client = caesar::P2PConnection{};

    server.stop();

    assert(!server.running());
    assert(server.peer_count() == 0);

    return 0;
}
