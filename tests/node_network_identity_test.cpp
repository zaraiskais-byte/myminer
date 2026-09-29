/*
 * Node network-identity test.
 *
 * CaesarNode now picks its genesis based on the network id it was
 * constructed with, and start() refuses to bring up a node whose
 * local storage contains a chain from a different network.
 *
 * This test covers three cases:
 *
 *   1. Constructing a node with NETWORK_MAINNET produces a chain
 *      whose genesis matches GENESIS_HASH_MAINNET.
 *
 *   2. Constructing a node with NETWORK_TESTNET produces a chain
 *      whose genesis matches GENESIS_HASH_TESTNET and differs from
 *      the Mainnet one.
 *
 *   3. Starting a Mainnet node against a data directory whose
 *      storage was written by a Testnet node throws a clear error
 *      instead of silently serving the wrong chain.
 */

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <stdexcept>

#include <caesar/blockchain_storage.hpp>
#include <caesar/network_params.hpp>
#include <caesar/node.hpp>

using namespace caesar;

namespace {

constexpr std::uint16_t PORT_A = 19511;
constexpr std::uint16_t PORT_B = 19512;

std::filesystem::path fresh_dir(const char* name) {
    const auto base =
        std::filesystem::temp_directory_path() / name;
    std::error_code ec;
    std::filesystem::remove_all(base, ec);
    return base;
}

} // namespace

int main() {

    // 1. Mainnet node uses Mainnet genesis.
    {
        const auto dir = fresh_dir("caesar_net_id_mainnet");

        CaesarNode node(dir, PORT_A, NETWORK_MAINNET);
        node.start();

        const auto chain = node.chain();
        assert(chain.size() == 1);
        assert(chain.front().hash() == GENESIS_HASH_MAINNET);

        node.stop();
        std::filesystem::remove_all(dir);
    }
    std::cout << "[net-id] mainnet genesis: OK\n";

    // 2. Testnet node uses Testnet genesis, which is distinct.
    {
        const auto dir = fresh_dir("caesar_net_id_testnet");

        CaesarNode node(dir, PORT_B, NETWORK_TESTNET);
        node.start();

        const auto chain = node.chain();
        assert(chain.size() == 1);
        assert(chain.front().hash() == GENESIS_HASH_TESTNET);
        assert(chain.front().hash() != GENESIS_HASH_MAINNET);

        node.stop();
        std::filesystem::remove_all(dir);
    }
    std::cout << "[net-id] testnet genesis: OK\n";

    // 3. Mainnet node refuses to start on Testnet storage.
    {
        const auto dir = fresh_dir("caesar_net_id_mismatch");

        // Create Testnet storage first.
        {
            CaesarNode testnet_node(dir, PORT_B, NETWORK_TESTNET);
            testnet_node.start();
            testnet_node.stop();
        }

        // Now try to open the same directory as Mainnet.
        bool threw = false;
        std::string message;
        try {
            CaesarNode mainnet_node(dir, PORT_A, NETWORK_MAINNET);
            mainnet_node.start();
        } catch (const std::exception& e) {
            threw = true;
            message = e.what();
        }
        assert(threw);
        assert(message.find("different network") !=
               std::string::npos);

        std::filesystem::remove_all(dir);
    }
    std::cout << "[net-id] mismatch rejected: OK\n";

    std::cout << "CaesarNodeNetworkIdentityTest: PASS\n";
    return 0;
}
