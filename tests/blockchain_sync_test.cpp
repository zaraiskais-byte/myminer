#include <chrono>
#include <filesystem>
#include <iostream>
#include <thread>

#include <caesar/node.hpp>

namespace {

bool wait_for_height(
    caesar::CaesarNode& node,
    std::size_t expected,
    int attempts = 300) {

    for (int i = 0; i < attempts; ++i) {
        if (node.height() >= expected)
            return true;

        std::this_thread::sleep_for(
            std::chrono::milliseconds(100));
    }

    return false;
}

}

int main() {
    const auto base =
        std::filesystem::temp_directory_path() /
        "caesar_blockchain_sync_test";

    std::error_code ec;
    std::filesystem::remove_all(base, ec);

    const auto node_a_dir = base / "node_a";
    const auto node_b_dir = base / "node_b";

    constexpr std::uint16_t port_a = 19544;
    constexpr std::uint16_t port_b = 19545;

    caesar::CaesarNode node_a(node_a_dir, port_a, 1);
    caesar::CaesarNode node_b(node_b_dir, port_b, 1);

    node_a.start();
    node_b.start();

    for (int i = 0; i < 2; ++i) {
        node_a.mine_one_block(
            "CAESAR_SYNC_TEST_MINER",
            1000000);
    }

    bool ok = true;

    if (node_a.height() != 2) {
        std::cerr << "FAIL: node_a height != 2\n";
        ok = false;
    }

    if (node_b.height() != 0) {
        std::cerr << "FAIL: node_b initial height != 0\n";
        ok = false;
    }

    const auto peer_id =
        node_b.connect_to_peer(
            "127.0.0.1",
            port_a);

    if (peer_id == 0) {
        std::cerr << "FAIL: peer connection failed\n";
        ok = false;
    }

    if (!wait_for_height(node_b, 2)) {
        std::cerr << "FAIL: node_b did not sync to height 2\n";
        ok = false;
    }

    const auto chain_a = node_a.chain();
    const auto chain_b = node_b.chain();

    const bool size_match =
        chain_a.size() == chain_b.size();

    bool hashes_match = size_match;

    if (size_match) {
        for (std::size_t i = 0; i < chain_a.size(); ++i) {
            if (chain_a[i].hash() != chain_b[i].hash()) {
                hashes_match = false;
                break;
            }
        }
    }

    std::cout << "SYNC_HEIGHT_A=" << node_a.height() << "\n";
    std::cout << "SYNC_HEIGHT_B=" << node_b.height() << "\n";
    std::cout << "CHAIN_SIZE_A=" << chain_a.size() << "\n";
    std::cout << "CHAIN_SIZE_B=" << chain_b.size() << "\n";
    std::cout << "CHAIN_HASHES_MATCH="
              << (hashes_match ? "YES" : "NO") << "\n";

    if (!size_match) {
        std::cerr << "FAIL: chain sizes differ\n";
        ok = false;
    }

    if (!hashes_match) {
        std::cerr << "FAIL: chain hashes differ\n";
        ok = false;
    }

    node_a.stop();
    node_b.stop();

    std::filesystem::remove_all(base, ec);

    if (!ok) {
        std::cout << "SYNC_TEST=FAIL\n";
        return 1;
    }

    std::cout << "SYNC_TEST=PASS\n";
    return 0;
}
