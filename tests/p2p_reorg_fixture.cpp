#include <filesystem>
#include <iostream>
#include <vector>

#include <caesar/block_builder.hpp>
#include <caesar/blockchain_storage.hpp>
#include <caesar/consensus.hpp>

namespace {

using namespace caesar;

Block make_genesis() {
    Block genesis;
    genesis.header.version = 1;
    genesis.header.height = 0;
    genesis.header.previous_hash = {};
    genesis.header.timestamp = 0;
    genesis.header.nonce = 0;
    genesis.header.difficulty = 0;

    Transaction marker;
    TransactionOutput output;
    output.amount = 1;
    output.recipient = "CAESAR_GENESIS_BURN";
    marker.outputs.push_back(output);
    genesis.transactions.push_back(marker);
    genesis.update_merkle_root();

    return genesis;
}

Block mine_block(
    const Block& previous,
    const std::string& recipient,
    std::uint64_t timestamp) {

    Block block = BlockBuilder::build(
        previous,
        {},
        recipient,
        timestamp,
        CZR_INITIAL_MINING_DIFFICULTY);

    if (!BlockBuilder::mine(block, 0, 1000000))
        throw std::runtime_error("fixture mining failed");

    return block;
}

void save_chain(
    const std::filesystem::path& path,
    const std::vector<Block>& chain) {

    BlockchainStorage storage(path);
    storage.save(chain);

    const auto loaded = storage.load();

    if (loaded.size() != chain.size())
        throw std::runtime_error("fixture reload size mismatch");

    for (std::size_t i = 0; i < chain.size(); ++i) {
        if (loaded[i].hash() != chain[i].hash())
            throw std::runtime_error("fixture reload hash mismatch");
    }
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr
            << "usage: caesar_p2p_reorg_fixture <output-dir>\n";
        return 2;
    }

    const std::filesystem::path output_dir = argv[1];

    std::error_code ec;
    std::filesystem::create_directories(output_dir, ec);
    if (ec) {
        std::cerr
            << "failed to create fixture directory: "
            << ec.message() << '\n';
        return 1;
    }

    try {
        const Block genesis = make_genesis();

        std::cerr << "MINE_A1\n";
        const Block a1 =
            mine_block(genesis, "CAESAR_REORG_A1", 1);

        std::cerr << "MINE_A2\n";
        const Block a2 =
            mine_block(a1, "CAESAR_REORG_A2", 2);

        std::cerr << "MINE_B1\n";
        const Block b1 =
            mine_block(genesis, "CAESAR_REORG_B1", 1);

        std::cerr << "MINE_B2\n";
        const Block b2 =
            mine_block(b1, "CAESAR_REORG_B2", 2);

        std::cerr << "MINE_B3\n";
        const Block b3 =
            mine_block(b2, "CAESAR_REORG_B3", 3);

        const std::vector<Block> chain_a{
            genesis, a1, a2
        };

        const std::vector<Block> chain_b{
            genesis, b1, b2, b3
        };

        if (!validate_block_chain(chain_a))
            throw std::runtime_error("chain A validation failed");

        if (!validate_block_chain(chain_b))
            throw std::runtime_error("chain B validation failed");

        if (calculate_chain_work(chain_b) <=
            calculate_chain_work(chain_a)) {
            throw std::runtime_error(
                "fixture chain B does not have greater work");
        }

        save_chain(
            output_dir / "node_a.blockchain.dat",
            chain_a);

        save_chain(
            output_dir / "node_b.blockchain.dat",
            chain_b);

        std::cout << "FIXTURE_A_BLOCKS="
                  << chain_a.size() << '\n';
        std::cout << "FIXTURE_B_BLOCKS="
                  << chain_b.size() << '\n';
        std::cout << "FIXTURE=PASS\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FIXTURE=FAIL: "
                  << e.what() << '\n';
        return 1;
    }
}
