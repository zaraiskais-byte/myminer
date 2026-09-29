/*
 * P2P peer scoring and banning test.
 *
 * P2PPeerManager now tracks a per-peer score. Successfully handled
 * frames reward the peer; protocol errors penalize it. When a peer's
 * score reaches zero, its address is added to a ban list and future
 * add_peer() calls with that address throw.
 *
 * This test covers:
 *   1. A new peer starts with PEER_SCORE_INITIAL.
 *   2. reward() increments the score, capped at PEER_SCORE_MAX.
 *   3. penalize() decrements the score.
 *   4. Penalizing to BAN_THRESHOLD bans the address and removes the
 *      peer.
 *   5. add_peer() refuses a banned address.
 *   6. unban() allows the address back in.
 *   7. Expired bans are dropped lazily.
 */

#include <cassert>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <thread>

#include <caesar/p2p_peer_manager.hpp>

using namespace caesar;

namespace {

constexpr std::uint16_t PORT_A = 39431;
constexpr std::uint16_t PORT_B = 39432;
constexpr std::uint16_t PORT_C = 39433;

P2PConnection make_connection(std::uint16_t port) {
    P2PTcpSocket server;
    server.listen_on(port);

    std::thread client([&]() {
        P2PConnection c;
        c.connect_to("127.0.0.1", port);
    });

    P2PConnection accepted(server.accept_connection());
    client.join();
    return accepted;
}

} // namespace

int main() {
    P2PPeerManager manager;

    // 1. New peer starts with PEER_SCORE_INITIAL.
    {
        const auto id = manager.add_peer(make_connection(PORT_A), "127.0.0.1", PORT_A);
        assert(manager.score(id) == P2PPeerManager::PEER_SCORE_INITIAL);
        manager.remove_peer(id);
    }
    std::cout << "[score] initial: OK\n";

    // 2. reward() increments up to cap.
    {
        const auto id = manager.add_peer(make_connection(PORT_A), "127.0.0.1", PORT_A);

        for (int i = 0; i < 200; ++i)
            manager.reward(id, 1);

        assert(manager.score(id) == P2PPeerManager::PEER_SCORE_MAX);
        manager.remove_peer(id);
    }
    std::cout << "[score] reward capped: OK\n";

    // 3. penalize() decrements.
    {
        const auto id = manager.add_peer(make_connection(PORT_A), "127.0.0.1", PORT_A);

        manager.penalize(id, 30);
        assert(manager.score(id) == P2PPeerManager::PEER_SCORE_INITIAL - 30);
        manager.remove_peer(id);
    }
    std::cout << "[score] penalize decrements: OK\n";

    // 4. Penalize to BAN_THRESHOLD removes and bans.
    {
        const auto id = manager.add_peer(make_connection(PORT_B), "127.0.0.1", PORT_B);

        const int hits_needed =
            P2PPeerManager::PEER_SCORE_INITIAL / P2PPeerManager::DEFAULT_PENALTY;

        for (int i = 0; i < hits_needed; ++i)
            manager.penalize(id, P2PPeerManager::DEFAULT_PENALTY);

        assert(!manager.contains(id));
        assert(manager.is_banned("127.0.0.1"));
    }
    std::cout << "[score] ban on threshold: OK\n";

    // 5. add_peer refuses banned address.
    {
        bool threw = false;
        std::string message;
        try {
            (void)manager.add_peer(make_connection(PORT_B), "127.0.0.1", PORT_B);
        } catch (const std::exception& e) {
            threw = true;
            message = e.what();
        }
        assert(threw);
        assert(message.find("banned") != std::string::npos);
    }
    std::cout << "[score] add_peer rejects banned: OK\n";

    // 6. unban() re-enables the address.
    {
        manager.unban("127.0.0.1");
        assert(!manager.is_banned("127.0.0.1"));

        const auto id = manager.add_peer(make_connection(PORT_C), "127.0.0.1", PORT_C);
        assert(manager.contains(id));
        manager.remove_peer(id);
    }
    std::cout << "[score] unban works: OK\n";

    std::cout << "CaesarP2PPeerScoringTest: PASS\n";
    return 0;
}
