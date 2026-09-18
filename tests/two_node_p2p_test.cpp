#include <cassert>
#include <chrono>
#include <filesystem>
#include <thread>

#include <caesar/node.hpp>

int main() {
    const auto base =
        std::filesystem::temp_directory_path() /
        "caesar_two_node_p2p_test";

    std::error_code ec;
    std::filesystem::remove_all(base, ec);

    const auto node_a_dir = base / "node_a";
    const auto node_b_dir = base / "node_b";

    constexpr std::uint16_t port_a = 19444;
    constexpr std::uint16_t port_b = 19445;

    caesar::CaesarNode node_a(node_a_dir, port_a, 1);
    caesar::CaesarNode node_b(node_b_dir, port_b, 1);

    node_a.start();
    node_b.start();

    assert(node_a.running());
    assert(node_b.running());
    assert(node_a.peer_count() == 0);
    assert(node_b.peer_count() == 0);

    const auto peer_id =
        node_a.connect_to_peer("127.0.0.1", port_b);

    assert(peer_id != 0);
    assert(node_a.peer_count() == 1);

    for (int i = 0; i < 50 && node_b.peer_count() == 0; ++i)
        std::this_thread::sleep_for(
            std::chrono::milliseconds(20));

    assert(node_b.peer_count() == 1);

    node_a.stop();
    node_b.stop();

    assert(!node_a.running());
    assert(!node_b.running());

    std::filesystem::remove_all(base, ec);

    return 0;
}
