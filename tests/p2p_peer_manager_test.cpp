#include <cassert>
#include <cstdint>
#include <thread>

#include <caesar/p2p_peer_manager.hpp>

int main() {
    using namespace caesar;

    constexpr std::uint16_t port = 39423;

    P2PTcpSocket server;
    server.listen_on(port);

    std::thread client_thread([&]() {
        P2PConnection client;
        client.connect_to("127.0.0.1", port);

        const P2PFrame frame = client.receive_frame();

        assert(frame.type == P2PMessageType::Transaction);
        assert(frame.payload.size() == 4);
        assert(frame.payload[0] == 0x43);
        assert(frame.payload[1] == 0x5A);
        assert(frame.payload[2] == 0x52);
        assert(frame.payload[3] == 0x01);
    });

    P2PConnection peer(server.accept_connection());

    P2PPeerManager manager;

    const std::uint64_t peer_id =
        manager.add_peer(
            std::move(peer),
            "127.0.0.1",
            port);

    assert(manager.size() == 1);
    assert(manager.contains(peer_id));
    assert(manager.info(peer_id).address == "127.0.0.1");
    assert(manager.info(peer_id).port == port);
    assert(manager.info(peer_id).connected);

    P2PFrame frame;
    frame.type = P2PMessageType::Transaction;
    frame.payload = {0x43, 0x5A, 0x52, 0x01};

    manager.send_to(peer_id, frame);

    client_thread.join();

    manager.remove_peer(peer_id);

    assert(manager.size() == 0);
    assert(!manager.contains(peer_id));

    return 0;
}
