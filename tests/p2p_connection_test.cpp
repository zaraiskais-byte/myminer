#include <cassert>
#include <cstdint>
#include <thread>
#include <vector>

#include <caesar/p2p_connection.hpp>

int main() {
    using namespace caesar;

    constexpr std::uint16_t port = 39422;

    P2PTcpSocket server_socket;
    server_socket.listen_on(port);

    std::thread client_thread([&]() {
        P2PConnection client;
        client.connect_to("127.0.0.1", port);

        P2PFrame request;
        request.type = P2PMessageType::Hello;
        request.payload = {0x43, 0x5A, 0x52, 0x01};

        client.send_frame(request);

        const P2PFrame response = client.receive_frame();

        assert(response.type == P2PMessageType::Pong);
        assert(response.payload.size() == 4);
        assert(response.payload[0] == 0x50);
        assert(response.payload[1] == 0x4F);
        assert(response.payload[2] == 0x4E);
        assert(response.payload[3] == 0x47);
    });

    P2PConnection peer(server_socket.accept_connection());

    const P2PFrame request = peer.receive_frame();

    assert(request.type == P2PMessageType::Hello);
    assert(request.payload.size() == 4);
    assert(request.payload[0] == 0x43);
    assert(request.payload[1] == 0x5A);
    assert(request.payload[2] == 0x52);
    assert(request.payload[3] == 0x01);

    P2PFrame response;
    response.type = P2PMessageType::Pong;
    response.payload = {0x50, 0x4F, 0x4E, 0x47};

    peer.send_frame(response);

    client_thread.join();

    return 0;
}
