#include <chrono>
#include <filesystem>
#include <iostream>
#include <thread>
#include <vector>

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

bool same_chain(
    const std::vector<caesar::Block>& a,
    const std::vector<caesar::Block>& b) {

    if (a.size() != b.size())
        return false;

    for (std::size_t i = 0; i < a.size(); ++i) {
        if (a[i].hash() != b[i].hash())
            return false;
    }

    return true;
}

}

int main() {
    const auto base =
        std::filesystem::temp_directory_path() /
        "caesar_p2p_reorg_test";

    const auto fixture_dir =
        std::filesystem::current_path() /
        "build" /
        "p2p_reorg_fixtures";

    std::error_code ec;
    std::filesystem::remove_all(base, ec);

    const auto node_a_dir = base / "node_a";
    const auto node_b_dir = base / "node_b";

    constexpr std::uint16_t port_a = 29644;
    constexpr std::uint16_t port_b = 29645;

    std::filesystem::create_directories(node_a_dir);
    std::filesystem::create_directories(node_b_dir);

    const auto fixture_a =
        fixture_dir / "node_a.blockchain.dat";
    const auto fixture_b =
        fixture_dir / "node_b.blockchain.dat";

    if (!std::filesystem::exists(fixture_a) ||
        !std::filesystem::exists(fixture_b)) {
        std::cerr
            << "FAIL: P2P reorg fixtures are missing\n"
            << "expected: "
            << fixture_a << "\n"
            << "expected: "
            << fixture_b << "\n";
        return 1;
    }

    std::filesystem::copy_file(
        fixture_a,
        node_a_dir / "blockchain.dat",
        std::filesystem::copy_options::overwrite_existing,
        ec);

    if (ec) {
        std::cerr
            << "FAIL: could not install node A fixture: "
            << ec.message() << "\n";
        return 1;
    }

    ec.clear();

    std::filesystem::copy_file(
        fixture_b,
        node_b_dir / "blockchain.dat",
        std::filesystem::copy_options::overwrite_existing,
        ec);

    if (ec) {
        std::cerr
            << "FAIL: could not install node B fixture: "
            << ec.message() << "\n";
        return 1;
    }

    caesar::CaesarNode node_a(node_a_dir, port_a, 1);
    caesar::CaesarNode node_b(node_b_dir, port_b, 1);

    std::cerr
        << "START_NODE_A port="
        << port_a
        << std::endl;

    node_a.start();

    std::cerr
        << "START_NODE_A_OK"
        << std::endl;

    std::cerr
        << "START_NODE_B port="
        << port_b
        << std::endl;

    node_b.start();

    std::cerr
        << "START_NODE_B_OK"
        << std::endl;

    bool ok = true;

    if (node_a.height() != 2) {
        std::cerr
            << "FAIL: node_a initial height != 2\n";
        ok = false;
    }

    if (node_b.height() != 3) {
        std::cerr
            << "FAIL: node_b initial height != 3\n";
        ok = false;
    }

    const auto branch_a = node_a.chain();
    const auto branch_b = node_b.chain();

    if (branch_a.size() != 3 ||
        branch_b.size() != 4) {

        std::cerr
            << "FAIL: unexpected branch sizes\n";
        ok = false;
    }

    if (branch_a.size() == branch_b.size() &&
        same_chain(branch_a, branch_b)) {

        std::cerr
            << "FAIL: branches are unexpectedly identical\n";
        ok = false;
    }

    /*
     * A initiates the connection. Its relay requests headers from B
     * and must discover B's stronger branch.
     */
    const auto peer_id =
        node_a.connect_to_peer(
            "127.0.0.1",
            port_b);

    if (peer_id == 0) {
        std::cerr
            << "FAIL: peer connection failed\n";
        ok = false;
    }

    if (!wait_for_height(node_a, 3)) {
        std::cerr
            << "FAIL: node_a did not reorg to height 3\n";
        ok = false;
    }

    const auto final_a = node_a.chain();
    const auto final_b = node_b.chain();

    std::cout
        << "REORG_HEIGHT_A="
        << node_a.height()
        << "\n";

    std::cout
        << "REORG_HEIGHT_B="
        << node_b.height()
        << "\n";

    const bool chains_match =
        same_chain(final_a, final_b);

    std::cout
        << "REORG_CHAIN_MATCH="
        << (chains_match ? "YES" : "NO")
        << "\n";

    if (!chains_match) {
        std::cerr
            << "FAIL: node_a did not adopt node_b chain\n";
        ok = false;
    }

    if (!final_a.empty() &&
        !branch_b.empty() &&
        final_a.back().hash() !=
            branch_b.back().hash()) {

        std::cerr
            << "FAIL: final tip is not node_b tip\n";
        ok = false;
    }

    std::cerr
        << "STOP_NODE_A"
        << std::endl;

    node_a.stop();

    std::cerr
        << "STOP_NODE_A_OK"
        << std::endl;

    std::cerr
        << "STOP_NODE_B"
        << std::endl;

    node_b.stop();

    std::cerr
        << "STOP_NODE_B_OK"
        << std::endl;

    std::filesystem::remove_all(base, ec);

    if (!ok) {
        std::cout
            << "REORG_TEST=FAIL\n";
        return 1;
    }

    std::cout
        << "REORG_TEST=PASS\n";
    return 0;
}
