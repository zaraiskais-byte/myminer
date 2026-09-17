#include <cassert>
#include <chrono>
#include <cstdint>
#include <thread>

#include <caesar/p2p_server.hpp>
#include <caesar/p2p_connection.hpp>
#include <caesar/p2p_handshake.hpp>

int main() {

    constexpr std::uint16_t port = 39424;

    caesar::P2PServer server;

    server.start(port, "127.0.0.1");

    assert(server.running());
    assert(server.peer_count() == 0);

    caesar::P2PConnection client;
    client.connect_to("127.0.0.1", port);
    caesar::P2PHello client_hello;
    caesar::perform_hello_handshake(client, client_hello, 1);

    for (int i = 0; i < 500; ++i) {
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
