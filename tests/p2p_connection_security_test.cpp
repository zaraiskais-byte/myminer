#include <cassert>
#include <cstdint>
#include <thread>
#include <vector>

#include <caesar/p2p_connection.hpp>

using namespace caesar;

static void send_raw(
    P2PTcpSocket& socket,
    const std::vector<std::uint8_t>& data) {

    socket.send_all(data.data(), data.size());
}

static std::vector<std::uint8_t> u32le(
    std::uint32_t value) {

    return {
        static_cast<std::uint8_t>(value & 0xff),
        static_cast<std::uint8_t>((value >> 8) & 0xff),
        static_cast<std::uint8_t>((value >> 16) & 0xff),
        static_cast<std::uint8_t>((value >> 24) & 0xff)
    };
}

static void expect_receive_reject(
    const std::vector<std::uint8_t>& attack) {

    constexpr std::uint16_t port = 39423;

    P2PTcpSocket listener;
    listener.listen_on(port);

    bool rejected = false;

    std::thread attacker([&]() {
        P2PTcpSocket socket;
        socket.connect_to("127.0.0.1", port);
        send_raw(socket, attack);
    });

    P2PConnection peer(
        listener.accept_connection());

    try {
        (void)peer.receive_frame();
    } catch (...) {
        rejected = true;
    }

    attacker.join();

    assert(rejected);
}

int main() {
    // 1. Payload length larger than the protocol limit.
    {
        auto attack =
            u32le(CZR_P2P_MAX_PAYLOAD + 1);

        attack.push_back(
            static_cast<std::uint8_t>(
                P2PMessageType::Hello));

        expect_receive_reject(attack);
    }

    // 2. Invalid message type.
    {
        auto attack = u32le(0);
        attack.push_back(0);

        expect_receive_reject(attack);
    }

    // 3. Invalid message type above the protocol range.
    {
        auto attack = u32le(0);
        attack.push_back(12);

        expect_receive_reject(attack);
    }

    // 4. Truncated frame: header advertises data
    //    that the attacker never sends.
    {
        auto attack = u32le(100);
        attack.push_back(
            static_cast<std::uint8_t>(
                P2PMessageType::Hello));

        expect_receive_reject(attack);
    }

    return 0;
}
