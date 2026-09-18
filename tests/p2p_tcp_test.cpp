#include <cassert>
#include <cstdint>
#include <stdexcept>
#include <thread>

#include <caesar/p2p_tcp.hpp>

int main() {
    using namespace caesar;

    constexpr std::uint16_t port = 39421;

    P2PTcpSocket server;
    server.listen_on(port);
    assert(server.valid());

    std::thread client_thread([&]() {
        P2PTcpSocket client;
        client.connect_to("127.0.0.1", port);

        const std::uint8_t message[] = {
            0x43, 0x5A, 0x52, 0x01
        };

        client.send_all(message, sizeof(message));

        std::uint8_t reply[4]{};
        client.receive_all(reply, sizeof(reply));

        assert(reply[0] == 0x50);
        assert(reply[1] == 0x4F);
        assert(reply[2] == 0x4E);
        assert(reply[3] == 0x47);
    });

    P2PTcpSocket peer = server.accept_connection();
    assert(peer.valid());
    assert(peer.peer_address() == "127.0.0.1");

    std::uint8_t received[4]{};
    peer.receive_all(received, sizeof(received));

    assert(received[0] == 0x43);
    assert(received[1] == 0x5A);
    assert(received[2] == 0x52);
    assert(received[3] == 0x01);

    const std::uint8_t reply[] = {
        0x50, 0x4F, 0x4E, 0x47
    };

    peer.send_all(reply, sizeof(reply));

    client_thread.join();

    return 0;
}
