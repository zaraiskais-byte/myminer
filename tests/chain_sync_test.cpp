#include <cassert>
#include <chrono>
#include <filesystem>
#include <thread>

#include <caesar/node.hpp>

int main() {

    const auto base =
        std::filesystem::temp_directory_path() /
        "caesar_chain_sync_test";

    std::error_code ec;
    std::filesystem::remove_all(base, ec);

    const auto node_a_dir = base / "node_a";
    const auto node_b_dir = base / "node_b";

    constexpr std::uint16_t port_a = 19446;
    constexpr std::uint16_t port_b = 19447;

    caesar::CaesarNode node_a(
        node_a_dir,
        port_a,
        1);

    caesar::CaesarNode node_b(
        node_b_dir,
        port_b,
        1);

    node_a.start();
    node_b.start();

    assert(node_a.height() == 0);
    assert(node_b.height() == 0);

    /*
     * Produce two real consecutive blocks on node A.
     * Node B remains at genesis and must obtain both blocks
     * through the actual P2P Headers -> GetBlocks -> Blocks path.
     */
    node_a.mine_one_block(
        "CAESAR_TEST_MINER",
        1000000);

    node_a.mine_one_block(
        "CAESAR_TEST_MINER",
        1000000);

    assert(node_a.height() == 2);
    assert(node_b.height() == 0);

    const auto source_chain = node_a.chain();
    assert(source_chain.size() == 3);
    assert(source_chain[0].header.height == 0);
    assert(source_chain[1].header.height == 1);
    assert(source_chain[2].header.height == 2);

    const auto source_tip_hash =
        source_chain.back().hash();

    node_a.connect_to_peer(
        "127.0.0.1",
        port_b);

    /*
     * Wait for the peer connection and then for the
     * complete block synchronization.
     */
    for (int i = 0; i < 300; ++i) {

        if (node_a.peer_count() == 1 &&
            node_b.peer_count() == 1 &&
            node_b.height() == 2) {
            break;
        }

        std::this_thread::sleep_for(
            std::chrono::milliseconds(100));
    }

    assert(node_a.peer_count() == 1);
    assert(node_b.peer_count() == 1);
    assert(node_b.height() == 2);

    const auto synced_chain = node_b.chain();

    assert(synced_chain.size() == 3);

    /*
     * Every block must match the source chain exactly.
     * This verifies the complete synchronized chain rather
     * than checking only the final tip.
     */
    for (std::size_t i = 0; i < source_chain.size(); ++i) {
        assert(synced_chain[i].hash() ==
               source_chain[i].hash());

        assert(synced_chain[i].header.height ==
               source_chain[i].header.height);
    }

    assert(synced_chain[0].header.height == 0);
    assert(synced_chain[1].header.height == 1);
    assert(synced_chain[2].header.height == 2);

    assert(
        synced_chain[1].header.previous_hash ==
        synced_chain[0].hash());

    assert(
        synced_chain[2].header.previous_hash ==
        synced_chain[1].hash());

    assert(synced_chain.back().hash() ==
           source_tip_hash);

    node_a.stop();
    node_b.stop();

    assert(!node_a.running());
    assert(!node_b.running());

    std::filesystem::remove_all(base, ec);

    return 0;
}
