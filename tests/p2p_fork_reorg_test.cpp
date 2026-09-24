#include <cassert>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <thread>
#include <vector>

#include <caesar/block_builder.hpp>
#include <caesar/blockchain_storage.hpp>
#include <caesar/node.hpp>

namespace {

caesar::Block mine_next(
    const caesar::Block& previous,
    std::uint64_t timestamp,
    const std::string& miner) {

    caesar::Mempool mempool;

    auto block = caesar::BlockBuilder::build(
        previous,
        mempool,
        miner,
        timestamp,
        caesar::CZR_INITIAL_MINING_DIFFICULTY);

    if (!caesar::BlockBuilder::mine(block, 0, 1000000))
        throw std::runtime_error("failed to mine test block");

    if (!caesar::validate_block_consensus(
            block,
            {previous},
            caesar::rebuild_utxo_set({previous}))) {
        throw std::runtime_error("mined test block failed consensus");
    }

    return block;
}

std::vector<caesar::Block> make_chain(
    const caesar::Block& genesis,
    std::size_t count,
    std::uint64_t timestamp_base,
    const std::string& miner) {

    std::vector<caesar::Block> chain{genesis};

    for (std::size_t i = 0; i < count; ++i) {
        const auto& previous = chain.back();

        caesar::Mempool mempool;

        auto block = caesar::BlockBuilder::build(
            previous,
            mempool,
            miner,
            timestamp_base + static_cast<std::uint64_t>(i),
            caesar::CZR_INITIAL_MINING_DIFFICULTY);

        if (!caesar::BlockBuilder::mine(
                block,
                0,
                1000000)) {
            throw std::runtime_error("failed to mine fork block");
        }

        chain.push_back(block);
    }

    if (!caesar::validate_candidate_chain(chain))
        throw std::runtime_error("prepared fork failed full-chain validation");

    return chain;
}

} // namespace

int main() {
    using namespace caesar;

    const auto base =
        std::filesystem::temp_directory_path() /
        "caesar_p2p_fork_reorg_test";

    std::error_code ec;
    std::filesystem::remove_all(base, ec);

    const auto node_a_dir = base / "node_a";
    const auto node_b_dir = base / "node_b";

    try {
        /*
         * Create the same canonical genesis that CaesarNode creates.
         */
        Block genesis;
        genesis.header.version = 1;
        genesis.header.height = 0;
        genesis.header.previous_hash = {};
        genesis.header.timestamp = 0;
        genesis.header.nonce = 0;
        genesis.header.difficulty = 0;

        Transaction genesis_tx;
        genesis_tx.outputs.push_back(
            TransactionOutput{1, "CAESAR_GENESIS_BURN"});
        genesis.transactions.push_back(genesis_tx);
        genesis.update_merkle_root();

        /*
         * A has a strictly higher-work competing branch:
         *
         * genesis -> A1 -> A2 -> A3
         *
         * B has:
         *
         * genesis -> B1 -> B2
         *
         * Both branches use real PoW.
         */
        const auto chain_a =
            make_chain(genesis, 3, 120, "P2P-FORK-A");

        const auto chain_b =
            make_chain(genesis, 2, 120, "P2P-FORK-B");

        BlockchainStorage storage_a(
            node_a_dir / "blockchain.dat");
        BlockchainStorage storage_b(
            node_b_dir / "blockchain.dat");

        std::filesystem::create_directories(node_a_dir);
        std::filesystem::create_directories(node_b_dir);

        storage_a.save(chain_a);
        storage_b.save(chain_b);

        CaesarNode node_a(node_a_dir, 19448, 1);
        CaesarNode node_b(node_b_dir, 19449, 1);

        node_a.start();
        node_b.start();

        assert(node_a.height() == 3);
        assert(node_b.height() == 2);

        const auto peer_id =
            node_b.connect_to_peer("127.0.0.1", 19448);

        assert(peer_id != 0);

        /*
         * request_headers() is triggered by the peer-added path.
         * Give the complete Headers -> GetBlocks -> Blocks ->
         * candidate validation -> replacement path time to finish.
         */
        bool replaced = false;

        for (int i = 0; i < 200; ++i) {
            if (node_b.height() == node_a.height() &&
                node_b.chain().back().hash() ==
                    node_a.chain().back().hash()) {
                replaced = true;
                break;
            }

            std::this_thread::sleep_for(
                std::chrono::milliseconds(50));
        }

        assert(replaced);

        const auto final_a = node_a.chain();
        const auto final_b = node_b.chain();

        assert(final_a.size() == 4);
        assert(final_b.size() == final_a.size());
        assert(final_b.back().hash() == final_a.back().hash());

        /*
         * Confirm that B really replaced its old branch rather than
         * merely extending it.
         */
        assert(final_b[1].hash() == final_a[1].hash());
        assert(final_b[2].hash() == final_a[2].hash());
        assert(final_b[3].hash() == final_a[3].hash());

        assert(final_b[1].hash() != chain_b[1].hash());
        assert(final_b[2].hash() != chain_b[2].hash());

        node_a.stop();
        node_b.stop();

        std::filesystem::remove_all(base, ec);

        std::cout << "P2P fork/reorg integration test passed\n";
        return 0;
    } catch (...) {
        std::filesystem::remove_all(base, ec);
        throw;
    }
}
