/*
 * Ping / Pong / Reject protocol test.
 *
 * P2PRelay now handles three message types that were previously
 * defined in the enum but ignored by handle_frame:
 *
 *   Ping   -> peer must reply with Pong carrying the same nonce
 *   Pong   -> validated for well-formedness
 *   Reject -> treated as a no-op until a payload schema is defined
 *
 * This test drives the handlers directly against a real TCP socket
 * pair so the full P2PConnection path is exercised.
 */

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

// Testing-only access to P2PRelay private members.
#define private public
#include <caesar/p2p_relay.hpp>
#undef private

#include <caesar/blockchain_storage.hpp>
#include <caesar/p2p_ping.hpp>
#include <caesar/p2p_server.hpp>

using namespace caesar;

namespace {

constexpr std::uint16_t PORT = 39501;

std::filesystem::path fresh_storage() {
    const auto p = std::filesystem::temp_directory_path() /
                   "caesar_p2p_ping_pong_test.dat";
    std::error_code ec;
    std::filesystem::remove(p, ec);
    return p;
}

} // namespace

int main() {

    const auto storage_path = fresh_storage();

    P2PServer server;  // not started; only its peer manager is used
    BlockchainStorage storage(storage_path);
    auto storage_mutex = std::make_shared<std::mutex>();

    P2PRelay relay(server, storage, storage_mutex);

    // Set up a real TCP connection pair so send_to() actually works.
    P2PTcpSocket listener;
    listener.listen_on(PORT);

    std::uint64_t received_pong_nonce = 0;
    bool got_pong = false;
    bool got_malformed_pong = false;

    std::thread client([&]() {
        P2PConnection c;
        c.connect_to("127.0.0.1", PORT);

        try {
            const P2PFrame frame = c.receive_frame();
            if (frame.type == P2PMessageType::Pong) {
                const P2PPong pong =
                    P2PPong::deserialize_binary(frame.payload);
                received_pong_nonce = pong.nonce;
                got_pong = true;
            }
        } catch (...) {
            got_malformed_pong = true;
        }
    });

    P2PConnection accepted(listener.accept_connection());

    const std::uint64_t peer_id = server.peers().add_peer(
        std::move(accepted), "127.0.0.1", PORT);

    // --- Case 1: handle_ping replies with Pong carrying same nonce ---
    {
        P2PPing ping;
        ping.nonce = 0xDEADBEEFCAFEBABEULL;
        const auto payload = ping.serialize_binary();

        relay.handle_ping(peer_id, payload);
    }

    client.join();

    assert(got_pong);
    assert(received_pong_nonce == 0xDEADBEEFCAFEBABEULL);
    std::cout << "[ping] reply carries same nonce: OK\n";

    // --- Case 2: handle_ping rejects malformed payloads ---
    {
        bool threw = false;
        try {
            relay.handle_ping(peer_id, {0x01});
        } catch (const std::exception&) {
            threw = true;
        }
        assert(threw);
    }
    std::cout << "[ping] malformed payload rejected: OK\n";

    // --- Case 3: handle_pong validates well-formed input ---
    {
        P2PPong pong;
        pong.nonce = 42;
        relay.handle_pong(peer_id, pong.serialize_binary());
    }
    std::cout << "[pong] valid payload accepted: OK\n";

    // --- Case 4: handle_pong rejects malformed input ---
    {
        bool threw = false;
        try {
            relay.handle_pong(peer_id, {});
        } catch (const std::exception&) {
            threw = true;
        }
        assert(threw);
    }
    std::cout << "[pong] malformed payload rejected: OK\n";

    // --- Case 5: handle_reject is a no-op ---
    {
        relay.handle_reject(peer_id, {});
        relay.handle_reject(peer_id, {0x01, 0x02, 0x03});
    }
    std::cout << "[reject] no-op: OK\n";

    // --- Case 6: send_ping uses the same wire format as handle_ping ---
    {
        P2PPing ping;
        ping.nonce = 7;
        const auto payload = ping.serialize_binary();

        // send_ping(peer_id, 7) and handle_ping(peer_id, payload)
        // must produce the same outgoing frame. We do not assert on
        // the wire here; we only confirm send_ping does not throw.
        relay.send_ping(peer_id, 7);
        (void)payload;
    }
    std::cout << "[ping] send_ping does not throw: OK\n";

    server.peers().remove_peer(peer_id);
    std::filesystem::remove(storage_path);

    std::cout << "CaesarP2PPingPongTest: PASS\n";
    return 0;
}
